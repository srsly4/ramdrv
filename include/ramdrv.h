#ifndef LIBRAMDRV_H
#define LIBRAMDRV_H

#define LIBRAMDRV_MAGIC 0xd34db33f

extern int ramdrv_open(void);

extern int ramdrv_close(int fd);

extern int ramdrv_inc(int fd);


#endif /* LIBRAMDRV_H */