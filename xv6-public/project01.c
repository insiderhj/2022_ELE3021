#include "types.h"
#include "stat.h"
#include "user.h"

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

  fd = open("users", O_RDONLY);
  read(fd, filestr, 500);
  printf(1, "after adduser: %s\n", filestr);
  close(fd);

  deleteUser("test2");

  fd = open("users", O_RDONLY);
  read(fd, filestr, 500);
  printf(1, "after deleteuser: %s\n", filestr);
  close(fd);

  exit();
}
