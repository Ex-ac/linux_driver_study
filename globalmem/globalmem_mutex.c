#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

#define GLOBALMEM_SIZE      0x1000
#define GLOBALMEM_MAJOR     230
#define GLOBALMEM_MAGIC     'g'
#define GLOBALMEM_MEM_CLEAR _IO(GLOBALMEM_MAGIC, 0)
#define GLOBALMEM_NUM       10

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev
{
    struct cdev cdev;
    char mem[GLOBALMEM_SIZE];
    struct mutex mutex;
};

static struct globalmem_dev *global_globalmem_devp;

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    int ret;
    unsigned long p = *ppos;
    struct globalmem_dev *devp = (struct globalmem_dev *)(filp->private_data);

    mutex_lock(globalmem_devp->mutex);

    if (p >= GLOBALMEM_SIZE)
    {
        mutex_unlock(globalmem_devp->mutex);
        return 0;
    }

    if (p > GLOBALMEM_SIZE - size)
    {
        p = GLOBALMEM_SIZE - p;
    }
    
    if (copy_to_user(buf, globalmem_devp->mem + p, size))
    {
        ret = -EFAULT;
    }
    else
    {
        *ppos += size;
        ret = size;
        printk(KERN_INFO "globalmem: Read %lu %byte(s) form %lu\n", size, p);
    }
    mutex_unlock(globalmem_devp->mutex);
}

static ssize_t globalmem_write(file *filp, const char __user *buf, size_t size, loft_t *ppos)
{
    int ret;
    unsigned long p = *ppos;
    struct globalmem_devp = (struct globalmem_dev *)(filp->private_data);
    
    mutex_lock(globalmem_devp->mutex);

    if (p >= GLOBALMEM_SIZE)
    {
        mutex_unlock(globalmem_devp->mutex);
        return 0;
    }

    if (p > GLOBALMEM_SIZE - size)
    {
        size = GLOBALMEM_SIZE - p;
    }

    if (copy_from_user(globalmem_devp->mem + p, buf, size))
    {
        ret = -EFAULT;
    }
    else
    {
        ret = size;
        *ppos += size;
        printk(KERN_INFO "globalmem: Write %lu byte(s) to %lu\n", size, p);
    }
    mutex_unlock(globalmem_devp->mutex);
    return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
    loff_t ret;

    switch (orig)
    {
        case 0:
            if (offset < 0 || offset > GLOBALMEM_SIZE)
            {
                return -EINVAL;
            }
            filp->f_pos = (unsigned int)(offset);
            ret = filp->f_pos;
            
            break;
        case 1:
            if (offset + filp->f_ops < 0 || offset + filp->f_ops > GLOBALMEM_SIZE)
            {
                return -EINVAL;
            }
            filp->f_pos += offset;
            ret = filp->f_pos;
            break;

        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}
