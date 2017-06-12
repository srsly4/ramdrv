#ifndef LIBRAMDRV_H
#define LIBRAMDRV_H

/* Opens Ramdrv control device */
extern int ramdrv_open(void);

/* Closes Ramdrv control device */
extern int ramdrv_close(int fd);

/* Creates ramdrv device on extisting control device with exact number of sectors */
extern int ramdrv_create(int fd, int sectors);

/* Deletes ramdrv device on existing control device with exact device number */
extern int ramdrv_delete(int fd, int dev_num);

#endif /* LIBRAMDRV_H */
