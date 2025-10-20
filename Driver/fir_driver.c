/*********************INCLUDES*********************/
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/of.h>

/********************MODULEINFO********************/
MODULE_AUTHOR("Anabela Stevic");
MODULE_DESCRIPTION("Driver for FIR Filter IP");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("custom:FIR");
MODULE_VERSION("1.0");

/********************CONSTANT*********************/
#define OK 0
#define ERR -1

#define DEVICE_NAME "fir"
#define DRIVER_NAME "fir_driver"
#define REGION_NAME "fir_region"
#define BUFFSIZE 64
#define DRVMEMSIZE 256

/*******************PROTOTYPES********************/
static int fir_open(struct inode *i, struct file *f);
static int fir_close(struct inode *i, struct file *f);
static ssize_t fir_read(struct file *f, char __user *buf, size_t len, loff_t *off);
static ssize_t fir_write(struct file *f, char __user *buf, size_t len, loff_t *off);
static int fir_probe(struct platform_device *pdev);
static int fir_remove(struct platform_device *pdev);
static int __init fir_init(void);
static void __exit fir_exit(void);

/********************GLOBALS*********************/
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = fir_open,
    .release = fir_close,
    .read = fir_read,
    .write = fir_write
};

static struct of_device_id fir_of_match[] = {
    {.compatible = "xlnx,FIR-1.0"},
    { /* NULL TERMINATOR */ }
};

static struct platform_driver fir_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = fir_of_match,
    },
    .probe = fir_probe,
    .remove = fir_remove
};
MODULE_DEVICE_TABLE(of, fir_of_match);

static dev_t fir_dev_id;
static struct class* fir_class;
static struct device* fir_device;
static struct cdev* fir_cdev;

static unsigned long fir_mem_start;
static unsigned long fir_mem_end;
static unsigned long fir_mem_size;
static void __iomem* fir_base_addr;
static unsigned long fir_curr_addr;

/********************FUNCTIONS*********************/
static int fir_open(struct inode *i, struct file *f) {
    printk(KERN_INFO "fir_open\n");
    return OK;
}

static int fir_close(struct inode *i, struct file *f) {
    printk(KERN_INFO "fir_close\n");
    return OK;
}

static ssize_t fir_read(struct file *f, char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "fir_read\n");

    int err = 0;
    unsigned int addr = 0;
    int val = 0;
    char output[BUFFSIZE];

    val = ioread32(fir_base_addr + fir_curr_addr);
    err = scnprintf(output, BUFFSIZE, "%d", val);
    if (!err) {
        printk(KERN_ERR "fir_read: Invalid data format from FIR.\n");
        return -EINVAL;
    }

    err = copy_to_user(buf, output, strlen(output) + 1);
    if (err) {
        printk(KERN_ERR "fir_read: Copy to user failed.\n");
        return -EINVAL;
    }

    if (fir_curr_addr == DRVMEMSIZE && val == 1) {
        fir_curr_addr = 0;
    } else {
		fir_curr_addr++;
	}
	
    return strlen(output);
}

static ssize_t fir_write(struct file *f, char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "fir_write\n");

    int err = 0;
    int val = 0;
    char input[BUFFSIZE];

    if (len > BUFFSIZE) {
        printk(KERN_WARNING "fir_write: User data too long, truncating.\n");
        len = BUFFSIZE - 1;
    }

    err = copy_from_user(input, buf, len);
    if (err) {
        printk(KERN_ERR "fir_write: Copy from user failed.\n");
        return -EINVAL;
    }
    input[len] = '\0';

    err = kstrtoint(input, 10, &val);
    if (err) {
        printk(KERN_ERR "fir_write: Invalid input format.\n");
        return -EINVAL;
    }

    iowrite32(val, fir_base_addr + fir_curr_addr);
    fir_curr_addr++;
    if (fir_curr_addr > DRVMEMSIZE) {
        printk(KERN_WARNING "fir_read: Too many writes, address out of bounds.\n");
        fir_curr_addr = DRVMEMSIZE;
    }

    return len;
}

static int fir_probe(struct platform_device *pdev) {
    printk(KERN_INFO "fir_probe\n");

    int err = 0;
    struct resource *r_mem;

    r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (r_mem == NULL) {
        printk(KERN_ERR "fir_probe: Failed to get resource.\n");
        return -EINVAL;
    }
    printk(KERN_INFO "fir_probe: Platform resource obtained.\n");

    fir_mem_start = r_mem->start;
    fir_mem_end = r_mem->end;
    fir_mem_size = fir_mem_end - fir_mem_start + 1;

    if (!request_mem_region(fir_mem_start, fir_mem_size, DRIVER_NAME)) {
        printk(KERN_ERR "fir_probe: Failed to get memory region.\n");
        return -EINVAL;
    }
    printk(KERN_INFO "fir_probe: Memory region obtained.\n");

    fir_base_addr = ioremap(fir_mem_start, fir_mem_size);
    if (fir_base_addr == NULL) {
        printk(KERN_ERR "fir_probe: Remap failed.\n");
        release_mem_region(fir_mem_start, fir_mem_size);
        return -EIO;
    }
    printk(KERN_INFO "fir_probe: FIR platform driver registered.\n");

    fir_curr_addr = 0;

    return OK;
}

static int fir_remove(struct platform_device *pdev){
    printk(KERN_INFO "fir_remove\n");

    iounmap(fir_base_addr);
    release_mem_region(fir_mem_start, fir_mem_size);
    printk(KERN_INFO "fir_remove: FIR platform driver removed.\n");

    return OK;
}

static int __init fir_init(void) {
    printk(KERN_INFO "fir_init: Initialize Module %s\n", DEVICE_NAME);

    int err = 0;

    err = alloc_chrdev_region(&fir_dev_id, 0, 1, REGION_NAME);
    if (err) {
        printk(KERN_ERR "fir_init: Failed registering character device.\n");
        return err;
    }
    printk(KERN_INFO "fir_init: Allocated character device.\n");

    fir_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (fir_class == NULL) {
        printk(KERN_ERR "fir_init: Failed class create.\n");
        goto unreg;
    }
    printk(KERN_INFO "fir_init: Class created.\n");

    fir_device = device_create(fir_class, NULL, MKDEV(MAJOR(fir_dev_id), 0), NULL, DEVICE_NAME);
    if (fir_device == NULL) {
        printk(KERN_ERR "fir_init: Failed device create.\n");
        goto unclass;
    }
    printk(KERN_INFO "fir_init: Device created.\n");

    fir_cdev = cdev_alloc();
    if (fir_cdev == NULL) {
        printk(KERN_ERR "fir_init: Failed to allocate character device.\n");
        goto undev;
    }
    fir_cdev->ops = &fops;
    fir_cdev->owner = THIS_MODULE;
    err = cdev_add(fir_cdev, fir_dev_id, 1);
    if (err) {
        printk(KERN_ERR "fir_init: Failed to add character device.\n");
        goto undev;
    }
    printk(KERN_INFO "fir_init: Added character device.\n");

    return platform_driver_register(&fir_driver);

undev:
    device_destroy(fir_class, MKDEV(MAJOR(fir_dev_id), 0));
unclass:
    class_destroy(fir_class);
unreg:
    unregister_chrdev_region(fir_dev_id, 1);
    return -EINVAL;
}

static void __exit fir_exit(void) {
    platform_driver_unregister(&fir_driver);
    cdev_del(fir_cdev);
    device_destroy(fir_class, MKDEV(MAJOR(fir_dev_id), 0));
    class_destroy(fir_class);
    unregister_chrdev_region(fir_dev_id, 1);

    printk(KERN_INFO "fir_exit: Exit Device Module \"%s\".\n", DEVICE_NAME);
}

/********************METADATA*********************/
module_init(fir_init);
module_exit(fir_exit);
