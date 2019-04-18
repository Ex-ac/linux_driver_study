#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/kernel.h>

#define GLOBALMEM_SIZE      0x1000
#define GLOBALMEM_MAJOR     230
#define GLOBALMEM_MAGIC     'g'
#define MEM_CLEAR           _IO(GLOBALMEM_MAGIC, 0)
#define DEVICE_NUM          10
// 设备号是设备的唯一标识
static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev
{
    struct cdev dev;
    unsigned char mem[GLOBALMEM_SIZE];
};
static struct globalmem_dev *globalmem_devp;

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    int ret;
    unsigned long p = *ppos;
    unsigned int count = size;

    struct globalmem_dev *devp = (struct globalmem_dev *)(filp->private_data);
    if (p >= GLOBALMEM_SIZE)
        return 0;

    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    if (copy_to_user(buf, devp->mem + p, count))
        ret = -EFAULT;
    else
    {
        *ppos = p + count;
        ret = count;

        printk(KERN_INFO "Read %u byte(s) from %lu\n", count, p);
    }
    return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    int ret;
    unsigned long p = *ppos;
    struct globalmem_dev *devp = (struct globalmem_dev *)(filp->private_data);
    // unsigned long p = *ppos;
    // unsigned int count = size;

    if (*ppos >= GLOBALMEM_SIZE)
        return 0;
    
    if (size > GLOBALMEM_SIZE - p)
        size = GLOBALMEM_SIZE - p;

    if (copy_from_user(devp->mem + p, buf, size))
        return -EFAULT;
    else
    {
        ret = size;
        *ppos = p + size;
        printk(KERN_INFO "Write %lu byte(s) to %lu\n", size, p);
    }
    return ret;
        
}

static loff_t globalmem_llseek(struct  file *filp, loff_t offset, int orig)
{
    loff_t ret;
    switch (orig)
    {
        case 0:
            if (offset <0 || offset > GLOBALMEM_SIZE)
                return -EINVAL;
            filp->f_pos = (unsigned int)(offset);
            ret = filp->f_pos;

            break;
        case 1:
            if (filp->f_pos + offset > GLOBALMEM_SIZE || filp->f_pos + offset < 0)
                return -EINVAL;
            filp->f_pos += offset;
            ret = filp->f_pos;
            break;

        default:
            return -EINVAL;
    }
    return ret;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret;
    struct globalmem_dev *devp = (struct globalmem_dev *)(filp->private_data);
    switch (cmd)
    {
        case MEM_CLEAR:
            memset(devp->mem, 0, GLOBALMEM_SIZE);
            printk(KERN_INFO "Globalmem is set to zero\n");
            ret = 0;
            break;
    
        default:
            ret = -EINVAL;
    }
    return ret;
}

static int globalmem_open(struct inode *inode, struct file *filp)
{
    struct globalmem_dev *devp = container_of(inode->i_cdev, struct globalmem_dev, dev);
    filp->private_data = devp;
    return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
    return 0;
}


static const struct file_operations globalmem_ops = 
{
    .owner = THIS_MODULE,
    .open = globalmem_open,
    .release = globalmem_release,
    .read = globalmem_read,
    .write = globalmem_write,
    .llseek = globalmem_llseek,
    .unlocked_ioctl = globalmem_ioctl
};


static void globalmem_setup_cdev(struct globalmem_dev *devp, int index)
{
    int err, devno = MKDEV(globalmem_major, index);

    cdev_init(&devp->dev, &globalmem_ops);
    devp->dev.owner = THIS_MODULE;
    err = cdev_add(&devp->dev, devno, 1);
    if (err)
        printk(KERN_INFO "Error %d adding globalmem", err, index);
}

static int __init globalmem_init(void)
{
    int ret;
    int i;
    dev_t devno = MKDEV(globalmem_major, 0);

    if (globalmem_major)
        ret = register_chrdev_region(devno, DEVICE_NUM, "globalmem");
    else
    {
        ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem");
        globalmem_major = MAJOR(devno);
    }
    
    if (ret < 0)
        return ret;
    
    globalmem_devp = kzalloc(sizeof(struct globalmem_dev) * DEVICE_NUM, GFP_KERNEL);
    if (!globalmem_devp)
    {
        unregister_chrdev_region(devno, DEVICE_NUM);
        return -ENOMEM;
    }
    for (i = 0; i < DEVICE_NUM; ++i)
    {
        globalmem_setup_cdev(globalmem_devp + i, i);

    }
    return 0;
}
module_init(globalmem_init);

static void __exit globalmem_exit(void)
{
    int i;
    for (i = 0; i < DEVICE_NUM; ++i)
    {
        cdev_del(&(globalmem_devp + i)->dev);
    }
    
    kfree(globalmem_devp);
    unregister_chrdev_region(MKDEV(globalmem_major, 0), DEVICE_NUM);
}

module_exit(globalmem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ex-ac ex-ac@outlook.com");