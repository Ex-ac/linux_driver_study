#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#define GLOBALMEM_SIZE 0x1000
#define GLOBALMEM_MAJOR 230
#define GLOBALMEM_MAGIC 'g'
#define MEM_CLEAR _IO(GLOBALMEM_MAGICm 0)

// 设备号是设备的唯一标识
static int globalmem_major = GLOBALMEM_MAJOR
module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev
{
    struct cdev cdev;
    unsigned char mem[GLOBALMEM_SIZE];
};

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    int ret;
    unsigned long p = *ppos;
    unsigned int count = size;

    globalmem_dev *devp = (struct globalmem_dev *)(filp->private_data);
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
    globalmem_dev *devp = (struct globalmem_dev *)(filp->private_data);
    // unsigned long p = *ppos;
    // unsigned int count = size;

    if (*ppos >= GLOBALMEM_SIZE)
        return 0;
    
    if (size > GLOBALMEM_SIZE - p)
        size = GLOBALMEM_SIZE - p;

    if (copy_from_user(devp->mem + p, buf, size)
        return -EFAULT;
    else
    {
        ret = size;
        *ppos = p + size;
        printk(KERN_INFO "Write %u byte(s) to %lu\n", size, p);
    }
    return ret;
        
}

static loof_t globalmem_llseek(struct  file *filp, loff_t offset, int orig)
{
    int ret;
    switch (orig)
    {
        case 0:
            if (offset <0 || offset > GLOBALMEM_SIZE)
                return -EINVAL;
            filp->f_pos = (unsigned int)(offset);
            ret = fip->f_ops;

            break;
        case 1:
            if (filp->f_ops + offset > GLOBALMEM_SIZE || filp->f_op + offset < 0)
                return -EINVAL;
            filp->f_ops += offset;
            ret = fip->f_ops;
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
            memset(devp->mem, GLOBALMEM_SIZE);
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
    filp->private_data = (void *)(globalmem_devp);
    return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
    return 0;
}
static struct globalmem_dev *globalmem_devp;

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


static void globalmem_setup_cdev(struct *devp, int index)
{
    int err, devno = MKDEV(globalmem_major, index);

    cdev_init(&devp->cdev, globalmem_ops);
    dev->owner = THIS_MODULE;
    err = cdev_add(&devp->cdev, devno, 1)
    if (err)
        printk(KERN_INFO "Error %d adding globalmem", err, index);
}

static int __init globalmem_init(void)
{
    int ret;
    dev_t devno = MKDEV(globalmem_major, 0);

    if (globalmem_major)
        ret = register_chrdev_region(devno, 1, "globalmem");
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "globalmem");
        globalmem_major = MAJOR(devno);
    }
    
    if (ret < 0)
        return ret;
    
    globalmem_devp = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
    if (!globalmem_devp)
    {
        unregister_chrdev_region(devno, 1);
        return -ENOMEM;
    }
    globalmem_setup_cdev();
    return 0;
}
module_init(globalmem_init);

static void __exit globalmem_exit(void)
{
    cdev_del(&globalmem_devp->cdev);
    kfree globalmem_devp;
    unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);
}

module_exit(globalmem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ex-ac ex-ac@outlook.com");