#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/types.h>

#include "../include/linux/ramdrv.h"

#define RAMDRV_MODULE_NAME "ramdrv"
#define RAMDRV_BLKDEV_NAME "ramdrv"

/* Module params */
static int logical_block_size = 512;
module_param(logical_block_size, int, 0);

static int sector_count = 1024;
module_param(sector_count, int, 0);


// block device major number
static int blkdev_id;

static struct file_operations ramdrv_dev_fops = {

};

/* === Driver initialization stuff ==== */

/* Misc device definition */
static struct miscdevice ramdrv_misc = {
        .minor = MISC_DYNAMIC_MINOR,
        .name  = RAMDRV_MODULE_NAME,
        .fops  = &ramdrv_dev_fops,
};


/* Driver init callback */
static int __init ramdrv_init(void) {
    int ret = 0;

    // register driver
    ret = misc_register(&ramdrv_misc);
    if (ret < 0)
      goto out;

    blkdev_id = register_blkdev(0, RAMDRV_BLKDEV_NAME);
    if (blkdev_id < 0){
      printk("Unable to register block device.\n");
      goto out;
    }

    printk("ramdrv module installed\n");

    out:
    return ret;
}

/* uninitialziation callback */
static void __exit ramdrv_exit(void) {
    unregister_blkdev(blkdev_id, RAMDRV_BLKDEV_NAME);
    misc_deregister(&ramdrv_misc);

    printk("ramdrv module uninstalled\n");
}

module_init(ramdrv_init);
module_exit(ramdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sample Ram Drive module");
MODULE_AUTHOR("Szymon Piechaczek");
