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

static int bytes_to_sectors_checked(unsigned long bytes)
{
	if( bytes % KERNEL_SECTOR_SIZE )
	{
		printk("***************WhatTheFuck***********************\n");
	}
	return bytes / KERNEL_SECTOR_SIZE;
}

static int sbull_open(struct block_device *dev, fmode_t mode){
  struct sbull_dev *sdev;
  printk(KERN_NOTICE "sbull opening!\n");
  sdev = dev->bd_disk->private_data;
  printk(KERN_NOTICE "sbull opened!\n");

  /*spin_lock(&sdev->lock);
  sdev->users++;
  spin_unlock(&sdev->lock);*/

  return 0;
}

static void sbull_release(struct gendisk *disk, fmode_t mode){
  struct sbull_dev *sdev;
  printk(KERN_NOTICE "sbull releasing!\n");
  sdev = disk->private_data;
  printk(KERN_NOTICE "sbull released!\n");

  /*spin_lock(&sdev->lock);
  sdev->users--;

  spin_unlock(&sdev->lock);*/
}

static int sbull_media_changed(struct gendisk *gd){
  return 0;
}

static int sbull_revalidate(struct gendisk *gd){
  return 0;
}


static int sbull_ioctl(struct block_device *dev, fmode_t mode,
                        unsigned cmd, unsigned long arg){
  //struct sbull_dev* sdev = dev->bd_disk->private_data;
  struct hd_geometry disk_geometry;
  printk(KERN_NOTICE "ramdrv: ioctl command!\n");
  switch(cmd){
    case HDIO_GETGEO:
    printk(KERN_NOTICE "ramdrv: getting geometry info\n");
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

// Device transfer request delivery
static void sbull_transfer(struct sbull_dev *dev, unsigned long sector,
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


static int sbull_xfer_bio(struct sbull_dev *dev, struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	sector_t sector = bio->bi_iter.bi_sector;

	/* Do each segment independently. */
	bio_for_each_segment(bvec, bio, iter) {
		char *buffer = __bio_kmap_atomic(bio, iter);
		sbull_transfer(dev, sector,bytes_to_sectors_checked(bio_cur_bytes(bio)),
				buffer, bio_data_dir(bio) == WRITE);
		sector += (bytes_to_sectors_checked(bio_cur_bytes(bio)));
		__bio_kunmap_atomic(bio);
	}
	return 0; /* Always "succeed" */
}


/*
 * Transfer a full request.
 */
static int sbull_xfer_request(struct sbull_dev *dev, struct request *req)
{
	struct bio *bio;
	int nsect = 0;

	__rq_for_each_bio(bio, req) {
		sbull_xfer_bio(dev, bio);
		nsect += bio->bi_iter.bi_size/KERNEL_SECTOR_SIZE;
	}
	return nsect;
}



/*
 * Smarter request function that "handles clustering".
 */
static void sbull_full_request(struct request_queue *q)
{
	struct request *req;
	struct sbull_dev *dev = q->queuedata;
	int ret;

	while ((req = blk_fetch_request(q)) != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
    if (req == NULL || (!(req->cmd_flags & REQ_OP_WRITE) && !(req->cmd_flags & REQ_OP_READ))) { //if it's not from-disk or to-disk request
#else
    if (req == NULL || req->cmd_type != REQ_TYPE_FS) {
#endif
			printk (KERN_NOTICE "Skip non-fs request\n");
			ret = -EIO;
			goto done;
		}
		sbull_xfer_request(dev, req);
		ret = 0;
	done:
		__blk_end_request_all(req, ret);
	}
}



/*
 * The direct make request version.
 */
static blk_qc_t sbull_make_request(struct request_queue *q, struct bio *bio)
{
	struct sbull_dev *dev = q->queuedata;
	int status;

	status = sbull_xfer_bio(dev, bio);
	bio->bi_error = status;
	bio_endio(bio);
	return BLK_QC_T_NONE;
}

// Device request handler (simple form)
static void sbull_request(struct request_queue *queue){
  struct request *req;
  struct sbull_dev *dev;
  int ret;
  //get anything from the queue
  while ((req = blk_fetch_request(queue)) != NULL){
    printk(KERN_NOTICE "Fetched request\n");
    dev = req->rq_disk->private_data;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
    if (req == NULL || (!(req->cmd_flags & REQ_OP_WRITE) && !(req->cmd_flags & REQ_OP_READ))) { //if it's not from-disk or to-disk request
#else
    if (req == NULL || req->cmd_type != REQ_TYPE_FS) {
#endif
      printk(KERN_NOTICE "Skipped non-fs request \n");
      ret = -EIO;
      goto done;
    }
    //move the data
    printk(KERN_NOTICE "Normal request\n");
    sbull_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req), bio_data(req->bio), rq_data_dir(req));
    ret = 0;

    done:
    if (!__blk_end_request_cur(req, ret)){
			req = blk_fetch_request(queue);
		}
  }
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

  // register block device
  blkdev_id = register_blkdev(0, RAMDRV_BLKDEV_NAME);
  if (blkdev_id < 0){
    printk(KERN_WARNING "ramdrv: unable to register block device.\n");
    goto out;
  }
  printk(KERN_NOTICE "ramdrv: initialized block device\n");


  //fill the sbull_dev struct
  ramdrv_sbull_dev = kmalloc(sizeof(struct sbull_dev), GFP_KERNEL);
  memset(ramdrv_sbull_dev, 0, sizeof(struct sbull_dev));


  printk(KERN_NOTICE "ramdrv: initialized sbull struct\n");

  ramdrv_sbull_dev->media_change = 0;
  ramdrv_sbull_dev->users = 0;
  ramdrv_sbull_dev->size = sector_count * KERNEL_SECTOR_SIZE;
  ramdrv_sbull_dev->data = vmalloc(ramdrv_sbull_dev->size); //allocate virtual disk size

  if (ramdrv_sbull_dev->data == NULL){
    printk(KERN_WARNING "ramdr: unable to valloc memory\n");
    goto vfree_out;
  }
  printk(KERN_NOTICE "ramdrv: valloced!\n");

  spin_lock_init(&(ramdrv_sbull_dev->lock));

  printk(KERN_NOTICE "ramdrv: spinlocked!\n");

  ramdrv_sbull_dev->queue = blk_alloc_queue(GFP_KERNEL);
	if (ramdrv_sbull_dev->queue == NULL)
		goto vfree_out;
	blk_queue_make_request(ramdrv_sbull_dev->queue, sbull_make_request);

  /*ramdrv_sbull_dev->queue = blk_init_queue(sbull_request, &(ramdrv_sbull_dev->lock));
  if (!(ramdrv_sbull_dev->queue)){
    printk(KERN_WARNING "ramdrv: unable to intialize request queue\n");
    goto vfree_out;
  }*/
  //blk_queue_hardsect_size(ramdrv_sbull_dev->queue, logical_block_size);
  blk_queue_logical_block_size(ramdrv_sbull_dev->queue, KERNEL_SECTOR_SIZE);
	ramdrv_sbull_dev->queue->queuedata = ramdrv_sbull_dev;

  printk(KERN_NOTICE "ramdrv: initialized sbull_dev\n");

  //gendisk initialization
  ramdrv_sbull_dev->gd = alloc_disk(1);
  if (!ramdrv_sbull_dev->gd) {
    printk(KERN_WARNING "alloc_disk failure\n");
    goto vfree_out;
  }

  ramdrv_sbull_dev->gd->major = blkdev_id;
  ramdrv_sbull_dev->gd->first_minor = 0;
  ramdrv_sbull_dev->gd->fops = &ramdrv_blkops;
  ramdrv_sbull_dev->gd->queue = ramdrv_sbull_dev->queue;
  ramdrv_sbull_dev->gd->private_data = (void*)ramdrv_sbull_dev;
  snprintf(ramdrv_sbull_dev->gd->disk_name, 32, "ramdrv");

  set_capacity(ramdrv_sbull_dev->gd, sector_count);
  add_disk(ramdrv_sbull_dev->gd);

  //finished
  printk(KERN_NOTICE "ramdrv: module installed\n");
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
    blk_cleanup_queue(ramdrv_sbull_dev->queue);
  }
  if (ramdrv_sbull_dev->data){
    vfree(ramdrv_sbull_dev->data);
    printk(KERN_NOTICE "ramdrv data destroyed\n");
  }

  unregister_blkdev(blkdev_id, RAMDRV_BLKDEV_NAME);
  kfree(ramdrv_sbull_dev);

  printk(KERN_NOTICE "ramdrv module uninstalled\n");
}

module_init(ramdrv_init);
module_exit(ramdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sample Ram Drive module");
MODULE_AUTHOR("Szymon Piechaczek");
