#include "scull.h"

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/stat.h>


int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_size = SCULL_SIZE;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_size, int, S_IRUGO);
module_param(scull_quantum, int, S_IRGUO);
module_param(scull_qset, int, S_IRUGO);

struct scull_dev *scull_devp;

int scull_trim(struct scull_dev *devp)
{
    struct scull_qset *next, *dptr;
    int qset = devp->qset;

    int i;

    for (dptr = devp->data; dptr; dptr = next)
    {
        if (dptr->data)
        {
            for (i = 0; i < qset; ++i)
            {
                kfree(dptr->data[i]);
            }
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }

    devp->size = 0;
    devp->quantum = scull_quantum;
    devp->qset = scull_qset;
    devp->data = NULL;
    return 0;
}

int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *devp;
    devp = container_of(indoe->i_cdev, struct scull_dev, cdev);
    flip->private_data = devp;

    if (filp->f_flags & O_ACCMODE == O_WRONLY)
    {
        if (down_interruptible(&devp->semaphore))
        {
            return -ERESTARTSYS;
        }
        scull_trim(devp);
        up(&devp->semaphore);
    }
    return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static scull_qset *scull_follow(struct scull_dev *devp, int n)
{
    struct scull_qset *qset = devp->data;

    if (qset == NULL)
    {
        qset = devp->data = kzalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (qset == NULL)
        {
            return NULL;
        }
    }

    while (n--)
    {
        if (qset->next == NULL)
        {
            qset->next = kzalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (qset->next == NULL)
            {
                return NULL;
            }
        }
        qset = qset->next;
    }
    return qset;
}

ssize_t scull_read(struct filp *filp, char __user 8buf, size_t count, loff_t * f_ops)
{
    struct scull_dev *devp = filp->private_data;
    struct scull_qset *dptr;
    int quantum = devp->quantum, qset = devp->qset;
    int item_size = quantum * qset;
    int item, s_pos, q_pos, rst;
    ssize_t ret = 0;

    if (down_interruptible(&dev->semaphore))
    {
        return -ERESTARTSYS;
    }
    if (*f_pos >= devp->size)
    {
        up(devp->semaphore);
        return 0;
    }

    item = (long)(*f_pos) / item_size;
    rest = (long)(*f_pos) % item_size;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    dptr = scull_follow(devp, item);

    if (dptr == NULL || dptr->data == NULL || dptr->data[s_pos] == NULL)
    {
        up(devp->semaphore);
        return -ERESTARTSYS;
    }

    if (count > quantum - q_pos)
    {
        count = quantum - q_pos;
    }

    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count))
    {
        ret = -EFAULT;
    }
    else
    {
        *f_pos += count;
        ret = count;
    }
    up(&devp->semaphore);
    return ret;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t * f_ops)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = devp->quantum, qset = devp->qset;
    int size = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t ret = 0;

    if (down_interruptible(&devp->semaphore))
    {
        return -ERESTARTSYS;
    }

    item = (long)(*f_pos) / size;
    rest = (long)(*f_pos) % size;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    dptr = scull_follow(devp, item);

    if (dptr == NULL)
    {
        up(&devp->semaphore);
        return ret;
    }

    if (dptr->data == NULL)
    {
        dptr->data = kzalloc(qset * sizeof(char *), GFP_KERNEL);
        if (dptr->data == NULL)
        {
            up(&devp->semaphore);
            return -ENOMEM;
        }
    }

    if (dptr->data[s_pos] == NULL)
    {
        dptr->data[s_pos] = kzalloc(quantum, GFP_KERNEL);

        if (dptr->data[s_pos] == NULL)
        {
            up(&devp->semaphore);
            return -ENOMEM;
        }
    }

    if (count > quantum - q_pos)
    {
        count = quantum - q_pos;
    }

    if (copy_from_user(dptr->data[s_pos] + q_pos, buf, count))
    {
        ret = -EFAULT;
    }
    else
    {
        *f_pos += count;
        ret = count;

        if (devp->size < *f_pos)
        {
            devp->size = *f_pos;
        }
    }

    up(&devp->semaphore);
    return ret;
}


struct file_operations scull_fops  = 
{
    .owner = THIS_MODULE,
    .open = scull_open,
    .release = scull_release,
    .read = scull_read,
    .write = scull_write,
};




static void scull_setup_cdev(struct scull_dev *devp, int index)
{
    int err, devno = MKDEV(scull_major, scull_minor + index);

    cdev_init(&devp->cdev, scull_fops);
    devp->cdev.owner = THIS_MODULE;
    devp->cdev.ops = &scull_fops;
    err = cdev_add(&devp->cdev, devno, 1);

    if (err)
    {
        printk(KERN_NOTICE "scull: %ld adding error, error code: %ld", index, err);
    }

}

static int __init scull_init(void)
{
    int ret, i;
    dev_t devno;

    if (scull_major == 0)
    {
        ret = alloc_chrdev_region(&devno, 0, scull_size, "scull");

        scull_major = MAJOR(devno);
    }
    else
    {
        devno = MKDEV(scull_major, scull_minor);
        ret = register_chrdev_region(devno, scull_size, "scull");
    }

    if (ret < 0)
    {
        printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
        return ret;
    }

    scull_devp = kzalloc(scull_size * sizeof(struct scull_dev), GFP_KERNEL);

    if (scull_devp == NULL)
    {
        unregister_chrdev_region(devno, scull_size);
        return -ENOMEM;
    }

    for (i = 0; i < scull_size; ++i)
    {
        scull_devp[i].quantum = scull_quantum;
        scull_devp[i].qset = scull_qset;
        sema_init(&scull_devp[i].semaphore, 1);
        scull_setup_cdev(scull_devp + i, i);
    }

    return 0;
}

module_init(scull_init);

static void __exit scull_exit(void)
{
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);

    for (i = 0; i < scull_size; ++i)
    {
        scull_trim(scull_devp + i);
        cdev_del(&(scull_devp + i)->cdev); 
    }
    kfree(scull_devp);


    unregister_chrdev_region(devno, scull_size);

}

module_exit(scull_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ex-ac ex-ac@outlook.com");