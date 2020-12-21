#include <fcntl.h> // open
#include <stdio.h>
#include <stdlib.h> // EXIT_FAILURE
#include <unistd.h> // write, exit
#include <string.h>

#define DEVICE_PATH "/dev/VDEV"

void error(char* msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

int main()
{
  int fd;

  fd = open(DEVICE_PATH, O_WRONLY);
  if (fd < 0)
    error("Device path not found");

  // Write config to device
  char* buffer = "ujhk";

  write(fd, buffer, strlen(buffer));

  close(fd);

  return 0;
}