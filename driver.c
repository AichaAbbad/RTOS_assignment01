#include<linux/module.h>
#include<linux/moduleparam.h>
#include<linux/init.h>
#include<linux/kernel.h>    
#include<linux/slab.h>        
#include<linux/fs.h>          
#include<linux/errno.h>       
#include<linux/types.h>       
#include<linux/proc_fs.h>
#include<linux/fcntl.h>       
#include<linux/seq_file.h>
#include<linux/cdev.h>
#include<linux/mutex.h>
#include<linux/semaphore.h>
#include<asm/uaccess.h>   

int simple_major =   0; // major number
int simple_minor =   0; // minor number
int memsize	= 255; // memory size

module_param(simple_major, int, S_IRUGO);
module_param(simple_minor, int, S_IRUGO);
module_param(memsize, int, S_IRUGO);

MODULE_AUTHOR("Abbad Aicha Manar");
MODULE_LICENSE("Dual BSD/GPL");

struct simple_dev{
    char *data;
    int memsize;
    struct semaphore sem;  
    struct cdev cdev;
};
struct simple_dev simple_device; 

// simple open
int simple_open(struct inode *node, struct file *filp){
    struct simple_dev *dev;
    dev = container_of(node -> i_cdev, struct simple_dev, cdev);
    filp -> private_data = dev; 
    return 0;
}

// simple release 
int simple_release(struct inode *node, struct file *filp){
    return 0;
}

// simple write
ssize_t simple_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_pos){
    struct simple_dev *dev = flip->private_data;
    ssize_t retval = 0;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

	if (count >= dev->memsize) 
        count = dev->memsize;

    if (copy_from_user(dev->data, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    // print the input message in the kernel
    printk("%s", dev->data);
    retval = count;
	out:
        up(&dev->sem);
        return retval;
}

struct file_operations simple_fops = {
    .owner =    THIS_MODULE,
    .write =    simple_write,
    .open =     simple_open,
    .release =  simple_release,
};

// initialization function
int simple_init_module(void){
    int error, result;
    dev_t dev = 0;

    if(simple_major){
        dev = MKDEV(simple_major,simple_minor);
        result = register_chrdev_region(dev, 1, "driver");
    }
    else{
        result = alloc_chrdev_region(&dev,simple_minor,1,"driver");
        simple_major = MAJOR(dev);
    }
    if(result < 0){
        printk(KERN_WARNING "The driver can't get the major number %d\n", simple_major);
        return result;
    }

    simple_device.memsize = memsize;
    simple_device.data = kmalloc(memsize * sizeof(char), GFP_KERNEL);
    memset(simple_device.data, 0, memsize * sizeof(char));

    sema_init(&simple_device.sem,1);

    cdev_init(&simple_device.cdev, &simple_fops);
    simple_device.cdev.owner = THIS_MODULE;
    simple_device.cdev.ops = &simple_fops;
    error = cdev_add (&simple_device.cdev, dev, 1);

    if(error){
        printk(KERN_NOTICE "Error %d adding simple", error);
    }
    else{
        printk(KERN_NOTICE "Simple Added major: %d minor: %d", simple_major, simple_minor);
    }
    return 0;
}
module_init(simple_init_module);

// cleanup function
void simple_cleanup_module(void){
    dev_t dev = MKDEV(simple_major,simple_minor);
    cdev_del(&simple_device.cdev);
    kfree(simple_device.data);
    unregister_chrdev_region(dev,1);
}
module_exit(simple_cleanup_module);