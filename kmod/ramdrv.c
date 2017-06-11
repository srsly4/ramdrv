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

/* ramdrv device definition */
static struct ramdrv_dev **devices;

// control device id
static int cntldev_id;
static char* cntldev_buff;
static size_t cntldev_buffsize;

void cntl_refresh_buff(void){
  int i = 0, devs = 0;
  if (cntldev_buff == NULL){
    cntldev_buff = kmalloc(128, GFP_KERNEL);
  }

  for (i = 0; i < RAMDRV_MINORS; i++){
    if (devices[i] != NULL){
      devs++;
    }
  }

  snprintf(cntldev_buff, 128, "ramdrv current devices: %d\n", devs);
  cntldev_buffsize = strlen(cntldev_buff+1);
}

//control function read - list of current ram drives
ssize_t cntl_read(struct file *f, char* user_buffer, size_t count, loff_t *position){
  if (cntldev_buff == NULL) return 0; //EOF
  if (*position >= cntldev_buffsize)
        return 0;
    /* If a user tries to read more than we have, read only as many bytes as we have */
  if (*position + count > cntldev_buffsize)
      count = cntldev_buffsize - *position;
  if( copy_to_user(user_buffer, cntldev_buff + *position, count) != 0 )
      return -EFAULT;

    *position += count;
    return count;
}


struct file_operations cntl_file_operations = {
  .owner = THIS_MODULE,
  .read = cntl_read
};



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

static int ramdrv_device_init(struct ramdrv_dev *dev, int sectors, int device_ndx){
  if (device_ndx >= RAMDRV_MINORS){
    printk(KERN_WARNING "ramdrv: cannot create more than 16 drives\n");
    return -1;
  }

  //fill data with zeros
  memset(dev, 0, sizeof(struct ramdrv_dev));

  //calculate the real size and alloc the data in memory
  dev->size = sectors * KERNEL_SECTOR_SIZE;
  if ((dev->data = vmalloc(dev->size)) == NULL){
    printk(KERN_ERR "ramdrv: could not allocate memory\n");
    return -1;
  }

  spin_lock_init(&dev->lock); //create spinlock...
  dev->queue = blk_alloc_queue(GFP_KERNEL); //...and request queue

  //handle making request, set queue block size
  blk_queue_make_request(dev->queue, ramdrv_make_request);
  blk_queue_logical_block_size(dev->queue, KERNEL_SECTOR_SIZE);
  dev->queue->queuedata = dev;

  //gendisk initialization
  dev->gd = alloc_disk(1);
  if (!dev->gd) {
    printk(KERN_ERR "ramdrv: alloc_disk failure\n");
    goto vfree_out;
  }

  dev->gd->major = blkdev_id;
  dev->gd->first_minor = device_ndx*RAMDRV_MINORS;
  dev->gd->fops = &ramdrv_blkops;
  dev->gd->queue = dev->queue;
  dev->gd->private_data = (void*)dev;
  snprintf(dev->gd->disk_name, 32, "ramdrv%c", device_ndx+'0');

  set_capacity(dev->gd, sectors);
  add_disk(dev->gd);
  printk(KERN_INFO "ramdrv: created device %s\n", dev->gd->disk_name);

  return 0; //success

  //failure
vfree_out:
  vfree(dev->data);
  return -1;

}

static void ramdrv_device_destroy(int ndx){
  printk(KERN_NOTICE "ramdrv: device %s has been destroyed\n", devices[ndx]->gd->disk_name);
  if (devices[ndx]->gd){
    del_gendisk(devices[ndx]->gd);
    put_disk(devices[ndx]->gd);
  }
  if (devices[ndx]->queue) {
    blk_cleanup_queue(devices[ndx]->queue);
  }
  if (devices[ndx]->data){
    vfree(devices[ndx]->data);
  }
  kfree(devices[ndx]);
  devices[ndx] = NULL;
}

/* Module initialization */
static int __init ramdrv_init(void) {
  int ret = 0;
  cntldev_buff = NULL;
  cntldev_buffsize = 0;

  //register control device
  cntldev_id = register_chrdev(0, RAMDRV_CNTLDEV_NAME, &cntl_file_operations);
  if (cntldev_id < 0){
    printk(KERN_ERR "ramdrv: unable to register control character device\n");
    goto out;
  }
  printk(KERN_INFO "ramdrv: initialized control device\n");

  // register block device
  blkdev_id = register_blkdev(0, RAMDRV_BLKDEV_NAME);
  if (blkdev_id < 0){
    printk(KERN_ERR "ramdrv: unable to register block device\n");
    goto out;
  }
  printk(KERN_INFO "ramdrv: initialized block device\n");

  //allocate table of devices with nulls
  devices = kmalloc(RAMDRV_MINORS * sizeof(struct ramdrv_dev*), GFP_KERNEL);
  memset(devices, 0, RAMDRV_MINORS * sizeof(struct ramdrv_dev*));

  devices[0] = kmalloc(sizeof(struct ramdrv_dev), GFP_KERNEL);
  ramdrv_device_init(devices[0], sector_count, 0);

  cntl_refresh_buff();
  //finished
  printk(KERN_INFO "ramdrv: module installed\n");

  out:
  return ret;
}

/* Module uninitialziation */
static void __exit ramdrv_exit(void) {
  int ndx = 0;
  //dealloc each device
  for (ndx = 0; ndx < RAMDRV_MINORS; ndx++){
    if (devices[ndx] == NULL) continue;
    ramdrv_device_destroy(ndx);
  }

  unregister_blkdev(blkdev_id, RAMDRV_BLKDEV_NAME);
  unregister_chrdev(cntldev_id, RAMDRV_CNTLDEV_NAME);
  kfree(devices);

  printk(KERN_NOTICE "ramdrv: module uninstalled\n");
}

module_init(ramdrv_init);
module_exit(ramdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple Ram Drive module");
MODULE_AUTHOR("Szymon Piechaczek");
