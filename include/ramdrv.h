#ifndef LIBRAMDRV_H
#define LIBRAMDRV_H


extern int ramdrv_open(void);

extern int ramdrv_close(int fd);

extern int ramdrv_create(int fd, int sectors);


#endif /* LIBRAMDRV_H */
