#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/timer.h>

#define SECOND_MAJOR 2048
static int global_second_major = SECOND_MAJOR;

module_param(global_second_major, int, S_IRUGO);

struct second_dev
{
    struct cdev cdev;
    atomic_t counter;
    struct timer_list s_timer;
};

static struct second_dev *global_second_devp;

static void sencod_timer_handler(unsigned long arg)
{
    mod_timer(&global_second_devp->s_timer, jiffies + HZ);

    atomic_inc(&global_second_devp->counter);

    printk(KERN_INFO "sencod: current jiffies is %ld\n", jiffies);
}

static int second_open(struct inode *inode, struct file *filp)
{

    init_timer(&global_second_devp->s_timer);
    global_second_devp->s_timer.function = &sencod_timer_handler;
    global_second_devp->s_timer.expires = jiffies + HZ;

    add_timer(&global_second_devp->s_timer);
    atomic_set(&global_second_devp->counter, 0);

    printk(KERN_INFO "second: Open\n");
    return 0;
}

static int second_release(struct inode *inode, struct file *filp)
{
    del_timer(&global_second_devp->s_timer);
    return 0;
}

static ssize_t second_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    int counter;

    counter = atomic_read(&global_second_devp->counter);

    if (put_user(counter, (int *)(buf)))
    {
        return -EFAULT;
    }
    else
    {
        return sizeof(int);
    }
}

static const struct file_operations global_second_ops = 
{
    .owner = THIS_MODULE,
    .open = second_open,
    .release = second_release,
    .read = second_read,
};


static void second_setup_cdev(struct second_dev *devp, int index)
{
    int err, devno = MKDEV(global_second_major, index);

    cdev_init(&devp->cdev, &global_second_ops);
    devp->cdev.owner = THIS_MODULE;
    err = cdev_add(&devp->cdev, devno, 1);

    if (err)
    {
        printk(KERN_INFO "second: Failed to add second device\n");
    }
    else
    {
        printk(KERN_INFO "second: Add second device success\n");
    }
    
}
static int __init second_init(void)
{
    int ret;
    dev_t devno = MKDEV(global_second_major, 0);
    if (global_second_major)
    {
        ret = register_chrdev_region(devno, 1, "second");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "second");
    }

    if (ret < 0)
    {
        return ret;
    }
    
    global_second_devp = kzalloc(sizeof(struct second_dev), GFP_KERNEL);

    if (global_second_devp == 0)
    {
        unregister_chrdev_region(devno, 1);
        return -ENOMEM;
    }
    second_setup_cdev(global_second_devp, 0);
    return 0;
}

module_init(second_init);

static void __exit second_exit(void)
{
    cdev_del(&global_second_devp->cdev);
    kfree(global_second_devp);
    unregister_chrdev_region(MKDEV(global_second_major, 0), 1);
}

module_exit(second_exit);

MODULE_AUTHOR("Ex-ac ex-ac@outlook.com");
MODULE_LICENSE("GPL v2");