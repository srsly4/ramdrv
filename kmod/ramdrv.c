#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/proc_fs.h>

#include "ramdrv_internal.h"
#include "../include/linux/ramdrv.h"



/* Module params */
static int logical_block_size = 512;
module_param(logical_block_size, int, 0);

static int sector_count = 1024;
module_param(sector_count, int, 0);


// block device major number
static int blkdev_id;

static struct block_device_operations ramdrv_blkops;

/* === Driver initialization stuff ==== */

/* Sbull device definition */
static struct sbull_dev ramdrv_sbull_dev;


/* Driver init callback */
static int __init ramdrv_init(void) {
    int ret = 0;

    // register block device
    blkdev_id = register_blkdev(0, RAMDRV_BLKDEV_NAME);
    if (blkdev_id < 0){
      printk(KERN_WARNING "sbull: unable to register block device.\n");
      goto out;
    }

    //fill the sbull_dev struct
    memset(&ramdrv_sbull_dev, 0, sizeof(struct sbull_dev));
    ramdrv_sbull_dev.size = sector_count * (logical_block_size / KERNEL_SECTOR_SIZE);
    ramdrv_sbull_dev.data = vmalloc(ramdrv_sbull_dev.size);
    if (ramdrv_sbull_dev.data == NULL){
      printk(KERN_WARNING "unable to valloc memory");
      goto out;
    }

    spin_lock_init(&ramdrv_sbull_dev.lock);
    //ramdrv_sbull_dev = blk_init_queue()

    printk("ramdrv module installed\n");

    out:
    return ret;
}

/* uninitialziation callback */
static void __exit ramdrv_exit(void) {
    unregister_blkdev(blkdev_id, RAMDRV_BLKDEV_NAME);
    
    printk("ramdrv module uninstalled\n");
}

module_init(ramdrv_init);
module_exit(ramdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sample Ram Drive module");
MODULE_AUTHOR("Szymon Piechaczek");
