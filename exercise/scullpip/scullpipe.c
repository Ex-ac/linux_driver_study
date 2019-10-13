#include "scullpipe.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/cdev.h>

#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/poll.h>

#include <linux/types.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/semaphore.h>
#include <linux/wait.h>

#include <linux/errno.h>

int scullpipe_major = SCULLPIPE_MAGIC;
module_param(scullpipe_major, int, S_IRUGO);

int scullpipe_minor = SCULLPIPE_MINOR;
module_param(scullpipe_minor, int, S_IRUGO);

int scullpipe_buff_size = SCULLPIPE_BUFF_SIZE;
module_param(scullpipe_buff_size, int, S_IRUGO);

int scullpipe_size = SCULLPIPE_SIZE;
module_param(scullpipe_size, int, S_IRUGO);

struct scullpipe_dev
{
    struct cdev cdev;
    int buff_size;
    int size;
    char *buff;
    char *r_ptr, *w_ptr;
    struct semaphore semaphore;
    wait_queue_head_t r_wait, w_wait;
    struct fasync_struct *fasync_queue;
};

static struct scullpipe_dev *scullpipe_devp;

// file operations
static int scullpipe_open(struct inode *inode, struct file *filp)
{
    struct scullpipe_dev *devp = container_of(inode->i_cdev, struct scullpipe_dev, cdev);

    filp->private_data = devp;

    if (down_interruptible(&devp->semaphore))
    {
        return -ERESTARTSYS;
    }
    if (devp->buff == NULL)
    {
        devp->buff = kzalloc(devp->buff_size, GFP_KERNEL);
        if (devp->buff == NULL)
        {
            up(&devp->semaphore);
            PRINT_DEBUG("not enough mem\n");
            return -ENOMEM;
        }
        devp->r_ptr = devp->w_ptr = devp->buff;
        devp->size = 0;
        PRINT_DEBUG("alloc %d byte\n", devp->buff_size);
        up(&devp->semaphore);
    }
    up(&devp->semaphore);
    return 0;
}

static int scullpipe_release(struct inode *inode, struct file *filp)
{
    struct scullpipe_dev *devp = filp->private_data;

    down(&devp->semaphore);

    if (devp->size == 0 && devp->buff)
    {
        kfree(devp->buff);
        devp->buff = NULL;
        PRINT_DEBUG("release free memory\n");
    }
    up(&devp->semaphore);
    return 0;
}

static ssize_t scullpipe_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    struct scullpipe_dev *devp = filp->private_data;
    ssize_t ret;
    char *dptr;
    int temp;

    if (down_interruptible(&devp->semaphore))
    {
        return -ERESTARTSYS;
    }

    while (devp->size == 0)
    {
        up(&devp->semaphore);

        if (filp->f_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }

        PRINT_DEBUG("\"%s\" reading: going to sleep\n", current->comm);

        if (wait_event_interruptible(devp->r_wait, devp->size > 0))
        {
            return -ERESTARTSYS;
        }

        if (down_interruptible(&devp->semaphore))
        {
            return -ERESTARTSYS;
        }
    }

    if (count > devp->size)
    {
        count = devp->size;
    }
    temp = 0;
    dptr = devp->r_ptr;
    if (count >= devp->buff_size + (devp->buff - devp->r_ptr))
    {
        temp = devp->buff_size + (devp->buff - devp->r_ptr);

        if (copy_to_user(buf, dptr, temp))
        {
            up(&devp->semaphore);
            return -EFAULT;
        }
        dptr = devp->buff;
        buf += temp;
    }

    temp = count - temp;

    if (copy_to_user(buf, dptr, temp))
    {
        up(&devp->semaphore);
        return -EFAULT;
    }

    ret = count;
    devp->r_ptr = dptr + temp;
    devp->size -= count;
    up(&devp->semaphore);

    PRINT_DEBUG("read %d bytes form memory\n", ret);
    wake_up_interruptible(&devp->w_wait);
    if (devp->fasync_queue)
    {
        kill_fasync(&devp->fasync_queue, SIGIO, POLLIN);
    }
    return ret;
}

static ssize_t scullpipe_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    struct scullpipe_dev *devp = filp->private_data;
    ssize_t ret;
    char *dptr;
    int temp;

    if (down_interruptible(&devp->semaphore))
    {
        return -ERESTARTSYS;
    }

    while (devp->size == devp->buff_size)
    {
        up(&devp->semaphore);
        if (filp->f_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }

        PRINT_DEBUG("\"%s\" wirte going to sleep\n", current->comm);
        if (wait_event_interruptible(devp->w_wait, devp->size < devp->buff_size))
        {
            return -ERESTARTSYS;
        }

        if (down_interruptible(&devp->semaphore))
        {
            return -ERESTARTSYS;
        }
    }

    if (count > devp->buff_size - devp->size)
    {
        count = devp->buff_size - devp->size;
    }
    temp = 0;
    dptr = devp->w_ptr;
    if (count >= devp->buff - (devp->w_ptr + devp->buff_size))
    {
        temp = devp->buff - (devp->w_ptr + devp->buff_size);
        if (copy_from_user(dptr, buf, temp))
        {
            up(&devp->semaphore);
            return -EFAULT;
        }
        dptr = devp->buff;
        buf += temp;
    }
    temp = count - temp;
    if (copy_from_user(dptr, buf, temp))
    {
        up(&devp->semaphore);
        return -EFAULT;
    }

    devp->size += temp;
    ret = count;
    devp->w_ptr = dptr + temp;
    up(&devp->semaphore);
    PRINT_DEBUG("write %d bytes to memory\n", count);
    wake_up_interruptible(&devp->r_wait);

    if (devp->fasync_queue)
    {
        kill_fasync(&devp->fasync_queue, SIGIO, POLL_OUT);
    }
    
    return ret;
}

static unsigned int scullpipe_poll(struct file *filp, poll_table *wait)
{
    struct scullpipe_dev *devp = filp->private_data;
    unsigned int ret;

    down(&devp->semaphore);
    poll_wait(filp, &devp->r_wait, wait);
    poll_wait(filp, &devp->w_wait, wait);

    if (devp->size > 0)
    {
        ret |= POLL_IN | POLLRDNORM;
    }
    if (devp->size != devp->buff_size)
    {
        ret |= POLL_OUT | POLLWRNORM;
    }
    
    up(&devp->semaphore);
    return ret;
}

static int scullpipe_fasync(int fd, struct file *filp, int mode)
{
    struct scullpipe_dev *devp = filp->private_data;
    return fasync_helper(fd, filp, mode, &devp->fasync_queue);
}


static const struct file_operations scullpipe_fops =
    {
        .owner = THIS_MODULE,
        .open = scullpipe_open,
        .release = scullpipe_release,
        .write = scullpipe_write,
        .read = scullpipe_read,
};

// proc
static void *scullpipe_seq_start(struct seq_file *sfilp, loff_t *pos)
{
    if (*pos >= scullpipe_size || *pos < 0)
    {
        return NULL;
    }
    return scullpipe_devp + *pos;
}

static void *scullpipe_seq_next(struct seq_file *sfilp, void *v, loff_t *pos)
{
    *pos++;
    if (*pos >= scullpipe_size || *pos < 0)
    {
        return NULL;
    }
    return scullpipe_devp + *pos;
}

static void scullpipe_seq_stop(struct seq_file *sfilp, void *v)
{
    return NULL;
}

static int scullpipe_seq_show(struct seq_file *sfilp, void *v)
{
    struct scullpipe_dev *devp = (struct scullpipe_dev *)(v);
    int i = 0;
    if (down_interruptible(&devp->semaphore))
    {
        return -ERESTARTSYS;
    }

    seq_printf(sfilp, "\nDevice %i, buff_size %d, size %d, buff start at %p, r_ptr %p, w_ptr %p\n", (devp - scullpipe_devp), devp->buff_size, devp->size, devp->buff, devp->r_ptr, devp->w_ptr);

    if (devp->buff)
    {
        seq_printf(sfilp, "Data:\n");
        for (; i < devp->buff_size; ++i)
        {
            seq_putc(sfilp, *(devp->buff + i));
        }
        seq_printf(sfilp, "\n");
    }

    up(&devp->semaphore);
    return 0;
}

static const struct seq_operations scullpipe_seq_ops =
    {
        .start = scullpipe_seq_start,
        .next = scullpipe_seq_next,
        .stop = scullpipe_seq_stop,
        .show = scullpipe_seq_show,
};

static int scullpipe_proc_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &scullpipe_seq_ops);
}

static const struct file_operations scullpipe_proc_fops =
    {
        .owner = THIS_MODULE,
        .open = scullpipe_proc_open,
        .release = seq_release,
        .read = seq_read,
        .llseek = seq_lseek,
};

static void scullpipe_creat_proc(void)
{
    struct proc_dir_entry *entry = proc_create("scullpipe", 0, NULL, &scullpipe_proc_fops);

    if (!entry)
    {
        PRINT_DEBUG("can't create proc file\n");
    }
    else
    {
        PRINT_DEBUG("create proc file\n");
    }
}

static void scullpipe_remove_proc(void)
{
    remove_proc_entry("scullpipe", NULL);
}

// init and remoeve device
static int scullpipe_setup_init(struct scullpipe_dev *devp, int index)
{
    dev_t devno = MKDEV(scullpipe_major, scullpipe_minor + index);
    int ret;

    devp->size = 0;
    devp->buff_size = scullpipe_buff_size;
    devp->buff = NULL;
    devp->r_ptr = devp->w_ptr = NULL;
    sema_init(&devp->semaphore, 1);
    init_waitqueue_head(&devp->r_wait);
    init_waitqueue_head(&devp->w_wait);

    cdev_init(&devp->cdev, &scullpipe_fops);
    devp->cdev.ops = &scullpipe_fops;

    ret = cdev_add(&devp->cdev, devno, 1);
    if (ret)
    {
        PRINT_DEBUG("%d can't add device, error code %d", ret);
        return ret;
    }
    {
        PRINT_DEBUG("%d add device finish\n");
        return ret;
    }
}

static int __init scullpipe_init(void)
{
    int ret, i;
    dev_t devno = MKDEV(scullpipe_major, scullpipe_minor);

    if (scullpipe_major == 0)
    {
        ret = alloc_chrdev_region(&devno, 0, scullpipe_size, "scullpipe");
        scullpipe_major = MAJOR(devno);
        scullpipe_minor = MINOR(devno);
    }
    else
    {
        ret = register_chrdev_region(devno, scullpipe_size, "scullpipe");
    }

    if (ret < 0)
    {
        PRINT_DEBUG("can't get major %d\n", scullpipe_major);
        return ret;
    }

    scullpipe_devp = kzalloc(sizeof(struct scullpipe_dev) * scullpipe_size, GFP_KERNEL);

    if (scullpipe_devp == NULL)
    {
        PRINT_DEBUG("not enough memory");
        return -ENOMEM;
    }

    for (i = 0; i < scullpipe_size; ++i)
    {
        scullpipe_setup_init(scullpipe_devp + i, i);
    }

    scullpipe_creat_proc();
    return 0;
}

module_init(scullpipe_init);

static void __exit scullpipe_exit(void)
{
    int i;

    scullpipe_remove_proc();

    for (i = 0; i < scullpipe_size; ++i)
    {
        cdev_del(&(scullpipe_devp + i)->cdev);
        if ((scullpipe_devp + i)->buff)
        {
           kfree((scullpipe_devp + i)->buff);
        }
    }
    kfree(scullpipe_devp);
    unregister_chrdev_region(MKDEV(scullpipe_major, scullpipe_minor), scullpipe_size);

    scullpipe_devp = NULL;
}

module_exit(scullpipe_exit);

MODULE_LICENSE("GPL v2");