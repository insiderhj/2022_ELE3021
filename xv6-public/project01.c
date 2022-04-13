#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char* argv[])
{
  printf(1, "My pid is %d\n", getpid());
  printf(1, "My ppid is %d\n", getppid());
  
  int pid = fork();
  if(pid == 0) {
	  for(int i = 0; i < 500; i++)
	  printf(1, "Child\n");
	  exit();
  }
  setpriority(pid, 2);
  for(int i = 0; i < 1000; i++) printf(1, "Parent\n");

  wait();
  exit();
}
