#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/unistd.h>


#include <linux/ramdrv.h>
#include <ramdrv.h>


int ramdrv_create(int sectors){
  int ret = 0;
  ramdrv_ioctl_create_t args;
  memset(&args, 0, sizeof(args));
}


int ramdrv_inc(int hw_fd){

    int ret = 0;

    ramdrv_ioctl_inc_t ioctl_args;

    memset(&ioctl_args, 0, sizeof(ioctl_args));

    ioctl_args.placeholder = LIBRAMDRV_MAGIC;

    ret = ioctl(hw_fd, RAMDRV_IOCTL_INCREMENT, &ioctl_args);

    return ret;
}
