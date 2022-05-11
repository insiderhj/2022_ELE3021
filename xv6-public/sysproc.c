#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_getppid(void)
{
  return myproc()->parent->proc->pid;
}

int
sys_yield(void)
{
  resetproc(myproc());
  yield();
  return 0;
}

int
sys_getlev(void)
{
  return myproc()->qlevel;
}

int
setpriority(int pid, int priority)
{
  if(priority < 0 || priority > 10) return -2;

  struct proc *p = getproc(pid);
  if(p->parent->proc->pid != myproc()->pid) return -1;

  p->priority = priority;
  return 0;
}

int
sys_setpriority(void)
{
  int pid;
  int priority;

  if(argint(0, &pid) < 0 || argint(1, &priority)) return -1;
  return setpriority(pid, priority);
}

int
sys_thread_create(void)
{
  int tid;
  int start_routine;
  int arg;

  if(argint(0, &tid) < 0 || argint(1, &start_routine) < 0 || argint(2, &arg) < 0) return -1;
  
  if (thread_create((thread_t*)tid, (void*)start_routine, (void*)arg)) return 0;
  return -1;
}

int
sys_thread_exit(void)
{
  int retval;

  if(argint(0, &retval) < 0) return -1;
  thread_exit((void*)retval);

  return 0;
}

int
sys_thread_join(void)
{
  int thread;
  int retval;

  if(argint(0, &thread) < 0 || argint(1, &retval) < 0) return -1;
  return thread_join((thread_t)thread, (void**)retval);
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  
  resetproc(myproc());
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
