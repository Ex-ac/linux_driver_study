#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/cdev.h>


static int faulty_major = 0;

static struct cdev *cdevp;

static ssize_t faulty_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;
    char stack_buf[4];

    memset(stack_buf, 0xff, 4);

    if (count > 4)
        count = 4;
    ret = copy_to_user(buf, stack_buf, count);
    
    if (!ret)
        return count;
    return ret;
}

static ssize_t faulty_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    *(int *) 0 = 0;
    return 0;
}

struct file_operations ops = 
{
    .owner = THIS_MODULE,
    .read = faulty_read,
    .write = faulty_write,
};

static int __init faulty_init(void)
{
    int ret;
    dev_t devno = MKDEV(faulty_major, 0);
    
    ret = alloc_chrdev_region(&devno, 0, 1, "faulty");
    if (ret < 0)
    {
        printk(KERN_INFO "faulty: alloc major faulty\n");
        return ret;
    }
    faulty_major = MAJOR(devno);

    cdevp = kzalloc(sizeof(struct cdev), GFP_KERNEL);

    if (cdevp == NULL)
    {
        printk(KERN_INFO "faulty: alloc memory faulty\n");
        unregister_chrdev_region(devno, 1);
        return -ENOMEM;
    }

    cdev_init(cdevp, &ops);
    cdevp->owner = THIS_MODULE;
    cdevp->ops = &ops;

    ret = cdev_add(cdevp, devno, 1);

    if (ret)
    {
        printk(KERN_INFO "faulty: can't not add char device, error code %d\n", ret);
        unregister_chrdev_region(devno, 1);
        kfree(cdevp);
        return ret;
    }

    return 0;
}

module_init(faulty_init);

static void __exit faulty_exit(void)
{
    cdev_del(cdevp);
    kfree(cdevp);
    unregister_chrdev_region(MKDEV(faulty_major, 0), 1);

}

module_exit(faulty_exit);


MODULE_LICENSE("GPLv2");
MODULE_AUTHOR("Ex-ac ex-ac@outlook.com");
