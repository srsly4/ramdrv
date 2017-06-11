#ifndef LIBRAMDRV_H
#define LIBRAMDRV_H


extern int ramdrv_open(void);

extern int ramdrv_close(int fd);

extern int ramdrv_create(int fd, int sectors);

extern int ramdrv_delete(int fd, int dev_num);


#endif /* LIBRAMDRV_H */
