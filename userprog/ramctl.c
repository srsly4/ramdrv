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

int fd;

void atexit_handler(void){
  ramdrv_close(fd);
}

int main(int argc, char** argv){
  int res;
  int num;
  char* endptr;

  if (argc != 3){
    fprintf(stderr, "Invalid number of arguments.\nUsage: <command> [devnum/sectors]\nCommands: create|delete\n");
    exit(EXIT_FAILURE);
  }

  fd = ramdrv_open();
  if (fd < 0) {
      fprintf(stderr, "Error while opening ramdrv control file: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
  }
  atexit(atexit_handler);

  num = (int)strtoul(argv[2], &endptr, 10);
  if (*endptr != '\0'){
      fprintf(stderr, "Invalid number.\n");
      exit(EXIT_FAILURE);
  }
  if (strcmp(argv[1], "create") == 0){
    if (num < 1024){
      fprintf(stderr, "Sector count too low (>1024=512KB)\n");
      exit(EXIT_FAILURE);
    }
    res = ramdrv_create(fd, 131072);
    if (res >= 0){
      printf("created device: ramdrv%c\n", '0' + res);
      exit(EXIT_SUCCESS);
    }
    else {
      fprintf(stderr, "Could not have created device, errno: %d\n", res);
      exit(EXIT_FAILURE);
    }
  }
  else if (strcmp(argv[2], "delete") == 0){
    if (num > 15){
      fprintf(stderr, "Invalid device number.\n");
      exit(EXIT_FAILURE);
    }
    res = ramdrv_delete(fd, num);
    if (res == 0){
      printf("Device successfully deleted.\n");
      exit(EXIT_SUCCESS);
    }
    else {
      fprintf(stderr, "Could not have deleted - errno: %d\n", res);
      exit(EXIT_FAILURE);
    }
  }
  else {
    fprintf(stderr, "Unknown command.\n");
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
