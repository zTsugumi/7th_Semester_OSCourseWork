#include <fcntl.h> // open
#include <stdio.h>
#include <stdlib.h> // EXIT_FAILURE
#include <string.h>
#include <unistd.h> // write, exit

#define DEVICE_PATH "/dev/VDEV"

void error(char* msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

static struct something {
  int a;
} a[1];

int main()
{
  int fd;

  fd = open(DEVICE_PATH, O_WRONLY);
  if (fd < 0)
    error("Device path not found");

  // Write config to device
  char* map = "0 ujhk";
  write(fd, map, strlen(map));

  char* spd = "1 50";
  write(fd, spd, strlen(spd));

  close(fd);

  return 0;
}