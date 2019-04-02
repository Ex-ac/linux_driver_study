#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/poll.h>

#define GLOBALFIFO_SIZE 0xff
#define GLOBALFIFO_MAJOR 231
#define GLOBALFIFO_MAGIC 'g'
#define GLOBALFIFO_CLEAR _IO(GLOBALFIFO_MAGIC, 0)
#define GLOBALFIFO_NUM 10

static int global_globalfifo_major = GLOBALFIFO_MAJOR;
module_param(global_globalfifo_major, int, S_IRUGO);

struct globalfifo_dev
{
    struct cdev cdev;
    char mem[GLOBALFIFO_SIZE];
    unsigned int current_len;
    struct mutex mutex;
    wait_queue_head_t r_wait_queue;
    wait_queue_head_t w_wait_queue;
};

struct globalfifo_dev *global_globalfifo_devp;

static ssize_t globalfifo_read(struct file *filp, char __user *buff, size_t size, loff_t *ppos)
{
    int ret;
    struct globalfifo_dev *devp = (struct globalfifo_dev *)(filp->private_data);
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&devp->mutex);
    add_wait_queue(&devp->r_wait_queue, &wait);
    while (devp->current_len == 0)
    {
        if (filp->f_flags & O_NONBLOCK)
        {
            mutex_unlock(&devp->mutex);
            remove_wait_queue(&devp->r_wait_queue, &wait);
            __set_current_state(TASK_RUNNING);
            return -EAGAIN;
        }

        mutex_unlock(&devp->mutex);
        __set_current_state(TASK_INTERRUPTIBLE);

        schedule();

        // if (signal_pending(current))
        // {
        //     remove_wait_queue(&devp->r_wait_queue, &wait);
        //     __set_current_state(TASK_RUNNING);
        //     return -ERESTARTSYS;
        // }
        mutex_lock(&devp->mutex);
    }

    if (size > devp->current_len)
    {
        size = devp->current_len;
    }

    if (copy_to_user(buff, devp->mem, size))
    {
        ret = -EFAULT;
    }
    else
    {
        memcpy(devp->mem, devp->mem + size, devp->current_len - size);
        devp->current_len -= size;
        printk(KERN_INFO "globalfifo: Read %lu byte(s), current len: %u\n", size, devp->current_len);
        ret = size;
        wake_up_interruptible(&devp->w_wait_queue);
    }

    mutex_unlock(&devp->mutex);
    remove_wait_queue(&devp->r_wait_queue, &wait);
    __set_current_state(TASK_RUNNING);

    return ret;
}

static ssize_t globalfifo_write(struct file *filp, const char __user *buff, size_t size, loff_t *ppos)
{
    int ret;
    struct globalfifo_dev *devp = (struct globalfifo_dev *)(filp->private_data);

    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&devp->mutex);
    add_wait_queue(&devp->w_wait_queue, &wait);

    while (devp->current_len == GLOBALFIFO_SIZE)
    {
        if (filp->f_flags & O_NONBLOCK)
        {
            mutex_unlock(&devp->mutex);
            remove_wait_queue(&devp->w_wait_queue, &wait);
            __set_current_state(TASK_RUNNING);
            return -EAGAIN;
        }

        __set_current_state(TASK_INTERRUPTIBLE);
        mutex_unlock(&devp->mutex);

        schedule();

        // if (signal_pending(current))
        // {
        //     remove_wait_queue(&devp->w_wait_queue, &wait);
        //     __set_current_state(TASK_RUNNING);
        //     return -ERESTARTSYS;
        // }

        mutex_lock(&devp->mutex);
    }

    if (size > GLOBALFIFO_SIZE - devp->current_len)
    {
        size = GLOBALFIFO_SIZE - devp->current_len;
    }

    if (copy_from_user(devp->mem + devp->current_len, buff, size))
    {
        ret = -EFAULT;
    }
    else
    {
        devp->current_len += size;
        ret = size;
        printk(KERN_INFO "globalfifo: Write %lu byte(s), current_len % u\n", size, devp->current_len);

        wake_up_interruptible(&devp->r_wait_queue);
    }

    mutex_unlock(&devp->mutex);
    remove_wait_queue(&devp->w_wait_queue, &wait);
    __set_current_state(TASK_RUNNING);

    return ret;
}

static loff_t globalfifo_llseek(struct file *filp, loff_t offset, int orgin)
{
    /* data */
    return 0;
};

static long globalfifo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret;
    struct globalfifo_dev *devp = filp->private_data;

    switch (cmd)
    {
    case GLOBALFIFO_CLEAR:
        mutex_lock(&devp->mutex);
        memset(devp->mem, 0, GLOBALFIFO_SIZE);
        devp->current_len = 0;
        mutex_unlock(&devp->mutex);
        wake_up_interruptible(&devp->w_wait_queue);
        ret = 0;
        break;

    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int globalfifo_open(struct inode *inode, struct file *filp)
{
    struct globalfifo_dev *devp = container_of(inode->i_cdev, struct globalfifo_dev, cdev);

    filp->private_data = devp;
    printk(KERN_INFO "globalfifo: Open\n");
    return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "globalfifo: Release\n");
    return 0;
}

static unsigned int globalfifo_poll(struct file *filp, poll_table *wait)
{
    unsigned int ret = 0;

    struct globalfifo_dev *devp = filp->private_data;

    mutex_lock(&devp->mutex);

    poll_wait(filp, &devp->r_wait_queue, wait);
    poll_wait(filp, &devp->w_wait_queue, wait);

    if (devp->current_len != 0)
    {
        ret |= POLL_IN | POLLRDNORM;
    }

    if (devp->current_len != GLOBALFIFO_SIZE)
    {
        ret |= POLL_OUT | POLLWRNORM;
    }

    mutex_unlock(&devp->mutex);
    return ret;
}

static const struct file_operations global_globalfifo_ops =
{
    .owner = THIS_MODULE,
    .open = globalfifo_open,
    .release = globalfifo_release,
    .read = globalfifo_read,
    .write = globalfifo_write,
    .llseek = globalfifo_llseek,
    .unlocked_ioctl = globalfifo_ioctl,
    .poll = globalfifo_poll,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *devp, int index)
{
    dev_t devno = MKDEV(global_globalfifo_major, index);
    int err;
    mutex_init(&devp->mutex);
    init_waitqueue_head(&devp->r_wait_queue);
    init_waitqueue_head(&devp->w_wait_queue);

    cdev_init(&devp->cdev, &global_globalfifo_ops);
    devp->cdev.owner = THIS_MODULE;
    err = cdev_add(&devp->cdev, devno, 1);
    if (err)
    {
        printk(KERN_INFO "globalfifo: Error %d adding globalfifo\n", err);
    }
    else
    {
        printk(KERN_INFO "globalfifo: %d globalfifo init finished\n", index);
    }
}

static int __init globalfifo_init(void)
{
    int ret;
    int i;

    dev_t devno = MKDEV(global_globalfifo_major, 0);

    if (global_globalfifo_major)
    {
        register_chrdev_region(devno, GLOBALFIFO_NUM, "globalfifo");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, GLOBALFIFO_NUM, "globalfifo");
        global_globalfifo_major = MAJOR(devno);
    }

    if (ret < 0)
    {
        printk(KERN_INFO "globalfifo: Can't alloc device number. Error code: %d\n", ret);
        return ret;
    }

    global_globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev) * GLOBALFIFO_NUM, GFP_KERNEL);

    if (!global_globalfifo_devp)
    {
        unregister_chrdev_region(devno, GLOBALFIFO_NUM);
        printk("globalfifo: No engouh memory\n");
        return -ENOMEM;
    }

    for (i = 0; i < GLOBALFIFO_NUM; ++i)
    {
        globalfifo_setup_cdev(global_globalfifo_devp + i, i);
    }

    return 0;
}

module_init(globalfifo_init);

static void __exit globalfifo_exit(void)
{
    int i;
    for (i = 0; i < GLOBALFIFO_NUM; ++i)
    {
        cdev_del(&((global_globalfifo_devp + i)->cdev));
    }
    unregister_chrdev_region(MKDEV(global_globalfifo_major, 0), GLOBALFIFO_NUM);
    kfree(global_globalfifo_devp);
}

module_exit(globalfifo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ex-ac ex-ac@outlook.com");