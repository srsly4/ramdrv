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

static int sbull_open(struct block_device *dev, fmode_t mode){
  return 0;
}

static void sbull_release(struct gendisk *disk, fmode_t mode){
  return;
}

static int sbull_media_changed(struct gendisk *gd){
  return 0;
}

static int sbull_revalidate(struct gendisk *gd){
  return 0;
}

static int sbull_ioctl(struct block_device *dev, fmode_t mode,
  unsigned cmd, unsigned long arg){
  return 0;
}

// Device request handler
static void sbull_request(struct request_queue *q){
  return;
}

// Device operations handler
static struct block_device_operations ramdrv_blkops = {
  .owner = THIS_MODULE,
	.open = sbull_open,
	.release = sbull_release,
	.media_changed = sbull_media_changed,
	.revalidate_disk = sbull_revalidate,
	.ioctl= sbull_ioctl
};

/* === Driver initialization stuff ==== */
/* Sbull device definition */
static struct sbull_dev *ramdrv_sbull_dev;


/* Driver init callback */
static int __init ramdrv_init(void) {
  int ret = 0;
  int which = 0;

  // register block device
  blkdev_id = register_blkdev(0, RAMDRV_BLKDEV_NAME);
  if (blkdev_id < 0){
    printk(KERN_WARNING "ramdrv: unable to register block device.\n");
    goto out;
  }

  printk("ramdrv: initialized block device");

  //fill the sbull_dev struct
  ramdrv_sbull_dev = kmalloc(sizeof(struct sbull_dev), GFP_KERNEL);
  memset(ramdrv_sbull_dev, 0, sizeof(struct sbull_dev));
  ramdrv_sbull_dev->size = sector_count * (logical_block_size / KERNEL_SECTOR_SIZE);
  ramdrv_sbull_dev->data = vmalloc(ramdrv_sbull_dev->size); //allocate virtual disk size
  if (ramdrv_sbull_dev->data == NULL){
    printk(KERN_WARNING "unable to valloc memory");
    goto vfree_out;
  }
  spin_lock_init(&(ramdrv_sbull_dev->lock));

  ramdrv_sbull_dev->queue = blk_init_queue(sbull_request, &(ramdrv_sbull_dev->lock));
  if (!(ramdrv_sbull_dev->queue)){
    printk(KERN_WARNING "unable to intialize request queue");
    goto vfree_out;
  }

  printk("ramdrv: initialized sbull_dev");

  //gendisk initialization
  ramdrv_sbull_dev->gd = alloc_disk(1);
  if (!ramdrv_sbull_dev->gd) {
    printk(KERN_WARNING "alloc_disk failure");
    goto vfree_out;
  }

  ramdrv_sbull_dev->gd->major = blkdev_id;
  ramdrv_sbull_dev->gd->first_minor = 0;
  ramdrv_sbull_dev->gd->fops = &ramdrv_blkops;
  ramdrv_sbull_dev->gd->queue = ramdrv_sbull_dev->queue;
  ramdrv_sbull_dev->gd->private_data = (void*)ramdrv_sbull_dev;
  snprintf(ramdrv_sbull_dev->gd->disk_name, 32, "ramdrv%c", 'a' + which);
  set_capacity(ramdrv_sbull_dev->gd, sector_count * (logical_block_size / KERNEL_SECTOR_SIZE));
  add_disk(ramdrv_sbull_dev->gd);

  //finished
  printk("ramdrv module installed\n");
  goto out;

  vfree_out:
  vfree(ramdrv_sbull_dev->data);

  out:
  return ret;
}

/* uninitialziation callback */
static void __exit ramdrv_exit(void) {
  if (ramdrv_sbull_dev->gd){
    del_gendisk(ramdrv_sbull_dev->gd);
    put_disk(ramdrv_sbull_dev->gd);
  }
  if (ramdrv_sbull_dev->queue) {
    blk_put_queue(ramdrv_sbull_dev->queue);
  }
  if (ramdrv_sbull_dev->data)
    vfree(ramdrv_sbull_dev->data);

  kfree(ramdrv_sbull_dev);

  unregister_blkdev(blkdev_id, RAMDRV_BLKDEV_NAME);

  printk("ramdrv module uninstalled\n");
}

module_init(ramdrv_init);
module_exit(ramdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sample Ram Drive module");
MODULE_AUTHOR("Szymon Piechaczek");
