#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct proc *runningproc[MLFQ_K];
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  struct proc *p;
  struct thread *t;

  initlock(&ptable.lock, "ptable");

  acquire(&ptable.lock);
  for(int i = 0; i < MLFQ_K; i++) {
    ptable.runningproc[i] = 0;
  }

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    for(t = p->threads; t < &p->threads[NTHREAD]; t++) {
      t->parent = p;
    }
  }
  release(&ptable.lock);
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

struct thread*
mythread(void) {
  struct cpu *c;
  struct thread *t;
  pushcli();
  c = mycpu();
  t = c->thread;
  popcli();
  return t;
}

struct proc*
getproc(int pid) {
  struct proc* p;
  int foundpid = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid) {
      foundpid = 1;
      break;
    }
  }

  if(!foundpid) return myproc();
  return p;
}

static struct thread*
allocthread(struct thread *t)
{
  char *sp;

  // Allocate kernel stack.
  if((t->kstack = kalloc()) == 0){
    t->state = UNUSED;
    return 0;
  }

  sp = t->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *t->tf;
  t->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *t->context;
  t->context = (struct context*)sp;
  memset(t->context, 0, sizeof *t->context);
  t->context->eip = (uint)forkret;

  return t;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  struct thread *t;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->threadcnt == 0)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->pid = nextpid++;

  // allocate main thread in p.threads[0] //hj
  t = allocthread(p->threads);
  if (t != 0) {
    p->threadcnt++;
    p->threadcnt = 1;
  }

  release(&ptable.lock);

  return p;
}

void resetproc(struct proc* p)
{
  acquire(&ptable.lock);
  if (ptable.runningproc[p->qlevel] == p)
    ptable.runningproc[p->qlevel] = 0;
  p->usedtq = 0;
  p->qlevel = 0;
  release(&ptable.lock);
}

void increasetq(struct proc *p)
{
  acquire(&ptable.lock);
  p->usedtq++;
  release(&ptable.lock);
}

void priorityboost(void)
{
  acquire(&ptable.lock);
  for(int i = 0; i < MLFQ_K; i++) {
    ptable.runningproc[i] = 0;
  }

  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    p->qlevel = 0;
    p->usedtq = 0;
  }
  release(&ptable.lock);
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  struct thread *t;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  t = p->threads;
  
  initproc = p;
  if((t->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(t->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  t->sz = PGSIZE;
  memset(t->tf, 0, sizeof(*t->tf));
  t->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  t->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  t->tf->es = t->tf->ds;
  t->tf->ss = t->tf->ds;
  t->tf->eflags = FL_IF;
  t->tf->esp = PGSIZE;
  t->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  t->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct thread *curthread = mythread();

  sz = curthread->sz;
  if(n > 0){
    if((sz = allocuvm(curthread->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curthread->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curthread->sz = sz;
  switchuvm(curthread);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct thread *nt;
  struct thread *tmpthread;
  struct proc *curproc = myproc();
  struct thread *ot;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  np->parent = curproc;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);
  np->threadcnt = curproc->threadcnt;
  
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  for(nt = np->threads, ot = curproc->threads; nt < &np->threads[NTHREAD]; nt++, ot++) {
    // Copy process state from proc.
    if((nt->pgdir = copyuvm(ot->pgdir, ot->sz)) == 0){
      for(tmpthread = np->threads; tmpthread < &np->threads[NTHREAD]; tmpthread++) {
        kfree(tmpthread->kstack);
        tmpthread->kstack = 0;
        tmpthread->state = UNUSED;
      }
      return -1;
    }
    nt->sz = ot->sz;
    *nt->tf = *ot->tf;

    // Clear %eax so that fork returns 0 in the child.
    nt->tf->eax = 0;

    if (ot->state == RUNNING) {
      acquire(&ptable.lock);

      nt->state = RUNNABLE;

      release(&ptable.lock);
    }
  }
  
  np->threadcnt = curproc->threadcnt;
  pid = np->pid;

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  struct thread *t;
  int iszombie;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init. 
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      iszombie = 0;
      for(t = p->threads; t < &p->threads[NTHREAD]; t++)
        if(t->state == ZOMBIE)
          iszombie = 1;
      
      if (iszombie)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  for(t = p->threads; t < &p->threads[NTHREAD]; t++)
    if(t->state != UNUSED)
      t->state = ZOMBIE;
  
  curproc->threadcnt = 0;

  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  struct thread *t;
  int havekids, pid, iszombie;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      iszombie = 0;
      for(t = p->threads; t < &p->threads[NTHREAD]; t++) {
        if(t->state == ZOMBIE){
          // Found one.
          kfree(t->kstack);
          t->kstack = 0;
          freevm(t->pgdir);
          t->state = UNUSED;
	
          iszombie = 1;
        }
      }

      if (iszombie) {
        pid = p->pid;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->threadcnt = 0;
        p->qlevel = 0;
        p->usedtq = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
	
void
scheduler(void)
{
  struct proc *p;
  struct thread *t;
  struct cpu *c = mycpu();
  c->proc = 0;
  c->thread = 0;
 
// Multilevel scheduler
#if SCHED_POLICY == MULTILEVEL_SCHED
  // int even_flag = 0;
  // for(;;){
  //   // Enable interrupts on this processor.
  //   sti();

  //   // Loop over process table looking for process to run.
  //   acquire(&ptable.lock);
  //   even_flag = 0;
  //   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
  //     if(p->pid % 2 != 0 || p->state != RUNNABLE) continue;

  //     // Switch to chosen process.  It is the process's job
  //     // to release ptable.lock and then reacquire it
  //     // before jumping back to us.

  //     even_flag = 1;

  //     c->proc = p;
  //     //TODO: c->thread = t;
  //     switchuvm(p);
  //     p->state = RUNNING;

  //     swtch(&(c->scheduler), p->context);
  //     switchkvm();

  //     // Process is done running for now.
  //     // It should have changed its p->state before coming back.
  //     c->proc = 0;
  //     c->thread = 0;
  //   }

  //   if (even_flag != 0) {
  //     release(&ptable.lock);
  //     continue;
  //   }

  //   struct proc* minproc;
  //   int foundmin = 0;

  //   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
  //     if(p->pid % 2 == 0 || p->state != RUNNABLE) continue;
  //     if(!foundmin) {
  //       minproc = p;
	// foundmin = 1;
  //     }
  //     if(p->pid < minproc->pid) minproc = p;
  //   }

  //   if (!foundmin) {
  //     release(&ptable.lock);
  //     continue;
  //   }

  //   c->proc = minproc;
  //   //TODO: c->thread = t;
  //   switchuvm(minproc);
  //   minproc->state = RUNNING;

  //   swtch(&(c->scheduler), minproc->context);
  //   switchkvm();

  //   c->proc = 0;
  //   //TODO: c->thread = t;
  //   release(&ptable.lock);
  // }

// MLFQ scheduler
#elif SCHED_POLICY == MLFQ_SCHED
  // for(;;){
  //   sti();

  //   acquire(&ptable.lock);
  //   for(int i = 0; i < MLFQ_K; i++) {
  //     int maxtq = i * 4 + 2;

  //     // vip: selected process to run
  //     struct proc *vip = 0;

  //     // if current level queue has running process
  //     if (ptable.runningproc[i] != 0 && ptable.runningproc[i]->state == RUNNABLE) {
        
	// // if time quantum has overed
  //       if(ptable.runningproc[i]->usedtq < maxtq) vip = ptable.runningproc[i];
	// else {
  //         ptable.runningproc[i]->qlevel++;
	//   ptable.runningproc[i]->usedtq = 0;
	//   ptable.runningproc[i] = 0;
	// }
  //     }

  //     // if current level queue doesn't have running process
  //     if(!vip) {
  //       for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
  //         if(p->state != RUNNABLE || p->qlevel != i) continue;
  // 	  if(!vip) {
  //           vip = p;
  // 	  }
	//   else if(vip->priority < p->priority) vip = p;
  //       }
  //     }

  //     // if cannot find process to run, move to next queue
  //     if(!vip) continue;

  //     ptable.runningproc[i] = vip;
  //     c->proc = vip;
  //     //TODO: c->thread=t;
  //     switchuvm(vip);
  //     vip->state = RUNNING;

  //     swtch(&(c->scheduler), vip->context);
  //     switchkvm();

  //     c->proc = 0;
  //     c->thread = 0;
  //     break;
  //   }
  //   release(&ptable.lock);
  // }

// default scheduler (round robin)
#else
  for(;;){

    sti();

    acquire(&ptable.lock); 
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      for(t = p->threads; t < &p->threads[NTHREAD]; t++) {
        if(t->state != RUNNABLE) continue;

        c->proc = p;
        c->thread = t;
        switchuvm(t);
        t->state = RUNNING;

        swtch(&(c->scheduler), t->context);
        switchkvm();

        c->proc = 0;
        c->thread = 0;
      }
    }
    release(&ptable.lock);
  }
#endif
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct thread *t = mythread();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(t->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&t->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  mythread()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  struct thread *t = mythread();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  t->chan = chan;
  t->state = SLEEPING;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  struct thread *t;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    for (t = p->threads; t < &p->threads[NTHREAD]; t++)
      if(t->state == SLEEPING && t->chan == chan)
        t->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;
  struct thread *t;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.

      for(t = p->threads; t < &p->threads[NTHREAD]; t++)
        if(t->state == SLEEPING)
          t->state = RUNNABLE;

      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  cprintf("procdump!\n");

  // TODO: 나중에 고치기
  // static char *states[] = {
  // [UNUSED]    "unused",
  // [EMBRYO]    "embryo",
  // [SLEEPING]  "sleep ",
  // [RUNNABLE]  "runble",
  // [RUNNING]   "run   ",
  // [ZOMBIE]    "zombie"
  // };
  // int i;
  // struct proc *p;
  // char *state;
  // uint pc[10];

  // for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
  //   if(p->state == UNUSED)
  //     continue;
  //   if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
  //     state = states[p->state];
  //   else
  //     state = "???";
  //   cprintf("%d %s %s", p->pid, state, p->name);
  //   if(p->state == SLEEPING){
  //     getcallerpcs((uint*)p->context->ebp+2, pc);
  //     for(i=0; i<10 && pc[i] != 0; i++)
  //       cprintf(" %p", pc[i]);
  //   }
  //   cprintf("\n");
  // }
}
