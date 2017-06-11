/*
 * helloworld user program
 *
 * Author: Dillon Hicks (hhicks@ittc.ku.edu)
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <errno.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <time.h>
#include <string.h>
#include <linux/ramdrv.h>
#include <ramdrv.h>


int main(int argc, char** argv){
  int fd;
  int i;

  fd = ramdrv_open();

  if (fd < 0) {
      fprintf(stderr, "Error while opening ramdrv control file: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
  }

  i = ramdrv_create(fd, 131072);
  printf("ramdrv_create returned: %d\n", i);

  ramdrv_close(fd);
  exit(EXIT_SUCCESS);
}
