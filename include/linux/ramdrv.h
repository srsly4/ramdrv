#ifndef RAMDRV_H
#define RAMDRV_H


/* Public api command structures */
typedef struct ramdrv_ioctl_create_s {
  int sectors;
  int index;
} ramdrv_ioctl_create_t;

typedef struct ramdrv_ioctl_delete_s {
  int index;
} ramdrv_ioctl_delete_t;

// generic union type
typedef union ramdrv_ioctl_param_u {
    ramdrv_ioctl_create_t create;
    ramdrv_ioctl_delete_t delete;
} ramdrv_ioctl_param_union;

// to identify the ioctl commands
#define RAMDRV_MAGIC 't'

// command macros
#define RAMDRV_IOCTL_DELETE _IOW(RAMDRV_MAGIC, 1, ramdrv_ioctl_delete_t)
#define RAMDRV_IOCTL_CREATE _IOWR(RAMDRV_MAGIC, 2, ramdrv_ioctl_create_t)

#define RAMDRV_IOC_MAX 2
#endif /* RAMDRV_H */
