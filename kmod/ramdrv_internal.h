#ifndef RAMDRV_INTERNAL_H
#define RAMDRV_INTERNAL_H

#define RAMDRV_MODULE_NAME "ramdrv"
#define RAMDRV_BLKDEV_NAME "ramdrv"

#define KERNEL_SECTOR_SIZE 512

#define SBULL_MINORS	16
#define MINOR_SHIFT	4
#define DEVNUM(kdevnum)	(MINOR(kdev_t_to_nr(kdevnum)) >> MINOR_SHIFT

struct sbull_dev {
        int size;                       /* Device size in sectors */
        u8 *data;                       /* The data array */
        short users;                    /* How many users */
        short media_change;             /* Flag a media change? */
        spinlock_t lock;                /* For mutual exclusion */
        struct request_queue *queue;    /* The device request queue */
        struct gendisk *gd;             /* The gendisk structure */
        struct timer_list timer;        /* For simulated media changes */
};

#endif
