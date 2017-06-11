#ifndef RAMDRV_H
#define RAMDRV_H
/*
 * ===============================================
 *             Public API Functions
 * ===============================================
 */

/*
 * There typically needs to be a struct definition for each flavor of
 * IOCTL call.
 */
typedef struct ramdrv_ioctl_inc_s {
    int placeholder;
} ramdrv_ioctl_inc_t;

typedef struct ramdrv_ioctl_create_s {
  int sectors;
} ramdrv_ioctl_create_t;

/*
 * This generic union allows us to make a more generic IOCTRL call
 * interface. Each per-IOCTL-flavor struct should be a member of this
 * union.
 */
typedef union ramdrv_ioctl_param_u {
    ramdrv_ioctl_inc_t set;
    ramdrv_ioctl_create_t create;
} ramdrv_ioctl_param_union;


/*
 * Used by _IOW to create the unique IOCTL call numbers. It appears
 * that this is supposed to be a single character from the examples I
 * have looked at so far.
 */
#define RAMDRV_MAGIC 't'

/*
 * For each flavor of IOCTL call you will need to make a macro that
 * calls the _IOW() macro. This macro is just a macro that creates a
 * unique ID for each type of IOCTL call. It uses a combination of bit
 * shifting and OR-ing of each of these arguments to create the
 * (hopefully) unique constants used for IOCTL command values.
 */
#define RAMDRV_IOCTL_INCREMENT	   _IOW(RAMDRV_MAGIC, 1, ramdrv_ioctl_inc_t)
#define RAMDRV_IOCTL_CREATE _IOW(RAMDRV_MAGIC, 2, ramdrv_ioctl_create_t)

#define RAMDRV_IOC_MAX 2
#endif /* RAMDRV_H */
