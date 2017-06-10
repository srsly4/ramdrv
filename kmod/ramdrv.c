#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/types.h>

#include "../include/linux/ramdrv.h"

#define RAMDRV_MODULE_NAME "ramdrv"


atomic_t ramdrv_message_count = ATOMIC_INIT(0);

struct proc_dir_entry *ramdrv_proc_entry;

static void ramdrv_inc_message_count(void){
    atomic_inc(&ramdrv_message_count);
}


/* Driver open callback */
static int ramdrv_open(struct inode *inode, struct file *file) {
    return 0;
}

/* Driver close callback */
static int ramdrv_close(struct inode *inode, struct file *file) {
    return 0;
}

static long ramdrv_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    int ret = 0;
    ramdrv_ioctl_param_union local_param;

    printk("ramdrv_ioctl()\n");

    if (copy_from_user((void *)&local_param, (void *)ioctl_param, _IOC_SIZE(ioctl_num)))
      return  -ENOMEM;

    switch (ioctl_num) {
        case RAMDRV_IOCTL_INCREMENT:
          ramdrv_inc_message_count();
          ret = 0;
        break;

        default:
          printk("ioctl: no such command\n");
          ret = -EINVAL;
    }

    return ret;
}



static int ramdrv_proc_read(char *buffer, off_t offset,
  int buffer_length, int *eof, void *data) {
    int ret;
    int i;

    /*
     * Obtain a local copy of the atomic variable that is
     * gaurenteed not to change while in the buffer formatting
     * logic..
     */
    int message_count = atomic_read(&ramdrv_message_count);

    printk("reading ramdrv proc entry.\n");

    /*
     * We give all of our information in one go, so if the user
     * asks us if we have more information the answer should
     * always be no.
     *
     * This is important because the standard read function from
     * the library would continue to issue the read system call
     * until the kernel replies that it has no more information,
     * or until its buffer is filled.
     *
     */
    if (offset > 0) {
        /* we have finished to read, return 0 */
        ret  = 0;
    } else {
        /* fill the buffer, return the buffer size This
         * assumes that the buffer passed in is big enough to
         * hold what we put in it. More defensive code would
         * check the input buffer_length parameter to check
         * the validity of that assumption.
         *
         * Note that the return value from this call (number
         * of characters written to the buffer) from this will
         * be added to the current offset at the file
         * descriptor level managed by the system code that is
         * called by this routine.
         */

        /*
         * Make sure we are starting off with a clean buffer;
         */
        strcpy(buffer, "");
        for (i = 0; i < message_count; i ++) {
            /*
             * Now that we are sure what the buffer
             * contains we can just append the message the
             * desired number of times. The third argument
             * to strncat() makes sure we do not go over
             * the length of the buffer.
             */
            buffer = strncat(buffer, "Hello World biatch!\n",
                             (buffer_length - strlen(buffer)) );
        }
        ret = strlen(buffer);
    }

    return ret;
}


/* File operations callback table */
struct file_operations
        ramdrv_dev_fops = {
        .owner          = THIS_MODULE,
        .unlocked_ioctl = ramdrv_ioctl,
        .open           = ramdrv_open,
        .release        = ramdrv_close,
};

/* Misc device definition */
static struct miscdevice
        ramdrv_misc = {
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

    //create /proc/ramdrv entry
    ramdrv_proc_entry = proc_create(RAMDRV_MODULE_NAME, S_IRUGO | S_IWUGO,
      NULL, &ramdrv_dev_fops);

    if (ramdrv_proc_entry == NULL) {
        // indicate initialization error
        ret = -ENOMEM;
        goto out;
    }

    printk("ramdrv module initialized\n");

    out:
    return ret;
}

/* uninitialziation callback */
static void __exit ramdrv_exit(void) {
    remove_proc_entry(RAMDRV_MODULE_NAME, ramdrv_proc_entry);
    misc_deregister(&ramdrv_misc);

    printk("ramdrv module uninstalled\n");
}

module_init(ramdrv_init);
module_exit(ramdrv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sample Ram Drive module");
MODULE_AUTHOR("Szymon Piechaczek");
