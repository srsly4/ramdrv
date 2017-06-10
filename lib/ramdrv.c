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


int ramdrv_open(void) {

    int fd = open("/dev/ramdrv", 'w');

    if (fd < 0)
        return -fd;

    return fd;
}


int ramdrv_close(int fd) {

    int ret = 0;
    ret = close(fd);
    return ret;
}


int ramdrv_inc(int hw_fd){

    int ret = 0;

    ramdrv_ioctl_inc_t ioctl_args;

    memset(&ioctl_args, 0, sizeof(ioctl_args));

    ioctl_args.placeholder = LIBRAMDRV_MAGIC;

    ret = ioctl(hw_fd, RAMDRV_IOCTL_INCREMENT, &ioctl_args);

    return ret;
}
