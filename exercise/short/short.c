
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include <linux/cdev.h>

#include <linux/slab.h>

#include <linux/uaccess.h>

#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/ioport.h>
#include <linux/iomap.h>
#include <linux/mm.h>

#include <linux/string.h>

#define SHORT_PORTS_SIZE 8

static int short_ports_size = SHORT_PORTS_SIZE;
module_param(short_ports_size, int, S_IRUGO);

static int major = 0;
module_param(major, int, S_IRUGO);

static int use_mem = 0;
module_param(use_mem, int, S_IRUGO);

static unsigned long base = 0x378;
unsigned long short_base = 0;
module_param(base, unsigned long, S_IRUGO);

struct resource *short_resource;

#undef PRINT_INFO
#define PRINT_INFO(fmt, args...) printk(KREN_INFO "short: " fmt, ##args)

struct short_dev
{
    cdev cdev;
};

static struct short_dev *global_short_devp;


static int short_open(struct inode *inode, struct file *filp)
{
    extern struct file_operations short_i_fops;

    if (iminor(inode) & 0x80)
    {
        filp->f_op = &short_i_fops;
    }
    return 0;
}

static int short_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static int short_poll(struct file *filp, poll_table *wait)
{
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}
static const struct file_operations short_fops =
{
        .owner = THIS_MODULE,
        .open = short_open,
        .release = short_release,

};

static void short_release_request(void)
{
    if (!use_mem)
    {
        release_region(short_base, short_ports_size);
    }
    else
    {
        iounremap(short_base);
        release_mem_region(short_base, short_ports_size);
    }
}

static int __init short_init(void)
{
    int ret;
    short_base = base;
    dev_t devno = MKDEV(major, 0);

    if (!use_mem)
    {
        short_resource = request_region(short_base, short_ports_size, "short");

        if (short_resource == NULL)
        {
            PRINT_INFO("can't get I/O port address: 0x:%lx\n", short_base);
            return -ENODEV;
        }
    }
    else
    {
        short_resource = request_mem_region(short_base, short_ports_size, "short");

        if (short_resource == NULL)
        {
            PRINT_INFO("can't get I/O mem address: 0x%lx\n", short_base);
        }
        return -ENODEV;

        short_base = (unsigned long)(ioremap_nocache(short_base, short_ports_size));
    }

    global_short_devp = kzalloc(sizeof(struct short_dev), GFP_KERNEL);
    if (global_short_devp == NULL)
    {
    }

    if (major == 0)
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "short");
    }
    else
    {
        ret = register_chrdev_region(devno, 1, "short");
    }

    if (ret < 0)
    {
        PRINT_INFO("can't register char no, %s", strerror(ret));
        unregister_chrdev_region(devno, 1);
        kfree(global_short_devp);
        short_release_request();
        return ret;
    }
    major = MAJOR(devno);

    cdev_init(&global_short_devp->cdev, &short_fops);
    global_short_devp->cdev.owner = THIS_MODULE;

    ret = cdev_add(&global_short_devp->cdev, devno, 1);

    if (ret)
    {
        PRINT_INFO("can't add char device, error code: %ld\n", ret);
        unregister_chrdev_region(devno, 1);
        kfree(global_short_devp);
        short_release_request();
        return ret;
    }

    return 0;
}
module_init(short_init);

static void __exit short_exit(void)
{
    cdev_del(&global_short_devp->cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
   
    kfree(global_short_devp);
    short_release_request();
}
module_exit(short_exit);

MODULE_LICENSE("GPL v2");