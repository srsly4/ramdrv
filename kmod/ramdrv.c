#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/version.h>

#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/proc_fs.h>
#include <linux/bio.h>

#include "ramdrv_internal.h"
#include "../include/linux/ramdrv.h"



/* Module params */
static int sector_count = 65536; // 128MB
module_param(sector_count, int, 0);


// block device major number
static int blkdev_id;

static int bytes_to_sectors_checked(unsigned long bytes) {
	if (bytes % KERNEL_SECTOR_SIZE)
		printk(KERN_WARNING "ramdrv: incorrect byte sector align!\n");
	return bytes / KERNEL_SECTOR_SIZE;
}

// Device open handler
static int ramdrv_open(struct block_device *dev, fmode_t mode){
  struct ramdrv_dev *opened_dev = dev->bd_disk->private_data;

  spin_lock(&opened_dev->lock);
  opened_dev->users++;
  spin_unlock(&opened_dev->lock);

  return 0;
}

// Device close handler
static void ramdrv_release(struct gendisk *disk, fmode_t mode){
  struct ramdrv_dev *dev = disk->private_data;

  spin_lock(&dev->lock);
  dev->users--;
  spin_unlock(&dev->lock);
}

// For Ramdrive media will never change as it's not a removable device
static int ramdrv_media_changed(struct gendisk *gd){ return 0; }
static int ramdrv_revalidate(struct gendisk *gd){ return 0; }

// Driver IOCTL handler
static int ramdrv_ioctl(struct block_device *dev, fmode_t mode,
                        unsigned cmd, unsigned long arg){
  struct hd_geometry disk_geometry;

  switch(cmd){
    //fake hard drive geometry for compability issues
    case HDIO_GETGEO:
    printk(KERN_NOTICE "ramdrv: getting faked geometry info\n");
    disk_geometry.cylinders = 512;
    disk_geometry.heads = 4;
    disk_geometry.sectors = 16;
    disk_geometry.start = 0;
    if (copy_to_user((void __user *) arg, &disk_geometry, sizeof(disk_geometry)))
            return -EFAULT;
    break;
  }

  return -ENOTTY;
}

// Actuall device transfer function
static void ramdrv_transfer(struct ramdrv_dev *dev, unsigned long sector,
        unsigned long nsect, char *buffer, int write) {
  unsigned long offset = sector*KERNEL_SECTOR_SIZE;
  unsigned long nbytes = nsect*KERNEL_SECTOR_SIZE;

  if ((offset + nbytes) > dev->size) {
    printk (KERN_NOTICE "ramdrv: Wrong offset! (%ld %ld)\n", offset, nbytes);
    return;
  }
  if (write)
    memcpy(dev->data + offset, buffer, nbytes);
  else
    memcpy(buffer, dev->data + offset, nbytes);
}


static int ramdrv_xfer_bio(struct ramdrv_dev *dev, struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	sector_t sector = bio->bi_iter.bi_sector;

	/* Do each segment independently. */
	bio_for_each_segment(bvec, bio, iter) {
		char *buffer = __bio_kmap_atomic(bio, iter);
		ramdrv_transfer(dev, sector,bytes_to_sectors_checked(bio_cur_bytes(bio)),
				buffer, bio_data_dir(bio) == WRITE);
		sector += (bytes_to_sectors_checked(bio_cur_bytes(bio)));
		__bio_kunmap_atomic(bio);
	}
	return 0; /* Always "succeed" */
}


/*
 * The direct make request version.
 * From Linux 4.4.0 API of elv make_request handler has changed
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
static blk_qc_t ramdrv_make_request(struct request_queue *q, struct bio *bio)
#else
static void ramdrv_make_request(struct request_queue *q, struct bio *bio)
#endif
{
	struct ramdrv_dev *dev = q->queuedata;
	int status;

	status = ramdrv_xfer_bio(dev, bio);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	bio->bi_error = status;
  bio_endio(bio);
#else
  bio_endio(bio, status);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	return BLK_QC_T_NONE;
#endif
}

// Device operations handler
static struct block_device_operations ramdrv_blkops = {
  .owner = THIS_MODULE,
	.open = ramdrv_open,
	.release = ramdrv_release,
	.media_changed = ramdrv_media_changed,
	.revalidate_disk = ramdrv_revalidate,
	.ioctl= ramdrv_ioctl
};


/* ramdrv device definition */
static struct ramdrv_dev *ramdrv_ramdrv_dev;

/* Module initialization */
static int __init ramdrv_init(void) {
  int ret = 0;

  // register block device
  blkdev_id = register_blkdev(0, RAMDRV_BLKDEV_NAME);
  if (blkdev_id < 0){
    printk(KERN_WARNING "ramdrv: unable to register block device.\n");
    goto out;
  }
  printk(KERN_NOTICE "ramdrv: initialized block device\n");


  //fill the ramdrv_dev struct
  ramdrv_ramdrv_dev = kmalloc(sizeof(struct ramdrv_dev), GFP_KERNEL);
  memset(ramdrv_ramdrv_dev, 0, sizeof(struct ramdrv_dev));


  printk(KERN_NOTICE "ramdrv: initialized ramdrv struct\n");

  ramdrv_ramdrv_dev->media_change = 0;
  ramdrv_ramdrv_dev->users = 0;
  ramdrv_ramdrv_dev->size = sector_count * KERNEL_SECTOR_SIZE;
  ramdrv_ramdrv_dev->data = vmalloc(ramdrv_ramdrv_dev->size); //allocate virtual disk size

  if (ramdrv_ramdrv_dev->data == NULL){
    printk(KERN_WARNING "ramdr: unable to valloc memory\n");
    goto vfree_out;
  }
  printk(KERN_NOTICE "ramdrv: valloced!\n");

  spin_lock_init(&(ramdrv_ramdrv_dev->lock));

  printk(KERN_NOTICE "ramdrv: spinlocked!\n");

  ramdrv_ramdrv_dev->queue = blk_alloc_queue(GFP_KERNEL);
	if (ramdrv_ramdrv_dev->queue == NULL)
		goto vfree_out;
	blk_queue_make_request(ramdrv_ramdrv_dev->queue, ramdrv_make_request);

  /*ramdrv_ramdrv_dev->queue = blk_init_queue(ramdrv_request, &(ramdrv_ramdrv_dev->lock));
  if (!(ramdrv_ramdrv_dev->queue)){
    printk(KERN_WARNING "ramdrv: unable to intialize request queue\n");
    goto vfree_out;
  }*/
  //blk_queue_hardsect_size(ramdrv_ramdrv_dev->queue, logical_block_size);
  blk_queue_logical_block_size(ramdrv_ramdrv_dev->queue, KERNEL_SECTOR_SIZE);
	ramdrv_ramdrv_dev->queue->queuedata = ramdrv_ramdrv_dev;

  printk(KERN_NOTICE "ramdrv: initialized ramdrv_dev\n");

  //gendisk initialization
  ramdrv_ramdrv_dev->gd = alloc_disk(1);
  if (!ramdrv_ramdrv_dev->gd) {
    printk(KERN_WARNING "alloc_disk failure\n");
    goto vfree_out;
  }

  ramdrv_ramdrv_dev->gd->major = blkdev_id;
  ramdrv_ramdrv_dev->gd->first_minor = 0;
  ramdrv_ramdrv_dev->gd->fops = &ramdrv_blkops;
  ramdrv_ramdrv_dev->gd->queue = ramdrv_ramdrv_dev->queue;
  ramdrv_ramdrv_dev->gd->private_data = (void*)ramdrv_ramdrv_dev;
  snprintf(ramdrv_ramdrv_dev->gd->disk_name, 32, "ramdrv");

  set_capacity(ramdrv_ramdrv_dev->gd, sector_count);
  add_disk(ramdrv_ramdrv_dev->gd);

  //finished
  printk(KERN_NOTICE "ramdrv: module installed\n");
  goto out;

  vfree_out:
  vfree(ramdrv_ramdrv_dev->data);

  out:
  return ret;
}

/* uninitialziation callback */
static void __exit ramdrv_exit(void) {
  if (ramdrv_ramdrv_dev->gd){
    del_gendisk(ramdrv_ramdrv_dev->gd);
    put_disk(ramdrv_ramdrv_dev->gd);
  }
  if (ramdrv_ramdrv_dev->queue) {
    blk_cleanup_queue(ramdrv_ramdrv_dev->queue);
  }
  if (ramdrv_ramdrv_dev->data){
    vfree(ramdrv_ramdrv_dev->data);
    printk(KERN_NOTICE "ramdrv data destroyed\n");
  }

  unregister_blkdev(blkdev_id, RAMDRV_BLKDEV_NAME);
  kfree(ramdrv_ramdrv_dev);

  printk(KERN_NOTICE "ramdrv module uninstalled\n");
}

module_init(ramdrv_init);
module_exit(ramdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sample Ram Drive module");
MODULE_AUTHOR("Szymon Piechaczek");
