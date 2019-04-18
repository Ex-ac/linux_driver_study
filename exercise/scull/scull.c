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

struct scull_dev *global_scull_devp;

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
        qset = devp->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (qset == NULL)
        {
            return NULL;
        }
        memset(qset, 0, sizeof(struct scull_qset));
    }

    while (n--)
    {
        if (qset->next == NULL)
        {
            qset->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (qset->next == NULL)
            {
                return NULL;
            }
            memset(qset->next, 0, sizeof(struct_qset));
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