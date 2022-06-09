#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
  char username[50], password[50];
  char users[500];
  int fd;
  
  if ((fd = open("users", O_RDONLY)) == -1) {
    fd = open("users", O_CREATE);
    close(fd);
    fd = open("users", O_WRONLY);
    write(fd, "root 0000\n", sizeof("root 0000\n"));
    close(fd);
    fd = open("users", O_RDONLY);
  } 
  read(fd, users, 500);
  close(fd);

  while (1) {
    printf(1, "Username: ");
    gets(username, sizeof(username));
    username[strlen(username) - 1] = 0;
    
    printf(1, "Password: ");
    gets(password, sizeof(password));
    password[strlen(password) - 1] = 0;


    if(login(users, username, password))
      printf(1, "try again\n");
    else
      break;
  }

  exec("sh", argv);
}