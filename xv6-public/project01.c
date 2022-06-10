#include "types.h"
#include "stat.h"
#include "user.h"
#define MODE_RUSR 32 // owner read
#define MODE_WUSR 16 // owner write
#define MODE_XUSR 8 // owner execute
#define MODE_ROTH 4 // others read
#define MODE_WOTH 2 // others write
#define MODE_XOTH 1 // others execute

int
main(int argc, char* argv[])
{
  char filestr[500];
  int fd;

  addUser("test1", "1234");

  fd = open("users", O_RDONLY);
  read(fd, filestr, 500);
  printf(1, "after adduser: %s\n", filestr);
  close(fd);

  addUser("test2", "1234");

  fd = open("users", O_RDONLY);
  read(fd, filestr, 500);
  printf(1, "after adduser: %s\n", filestr);
  close(fd);

  addUser("test3", "1234");
  addUser("test4", "1234");
  addUser("test5", "1234");
  addUser("test6", "1234");
  addUser("test7", "1234");
  addUser("test8", "1234");
  addUser("test9", "1234");
  addUser("test10", "1234");

  fd = open("users", O_RDONLY);
  read(fd, filestr, 500);
  printf(1, "after adduser: %s\n", filestr);
  close(fd);

  deleteUser("test2");

  fd = open("users", O_RDONLY);
  read(fd, filestr, 500);
  printf(1, "after deleteuser: %s\n", filestr);
  close(fd);
  addUser("test11", "1234");

  chmod("/test1", MODE_XOTH);

  exit();
}
