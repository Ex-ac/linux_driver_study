#include "scullc.h"

#include <linux/init.h>

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/fs.h>

#include <linux/fcntl.h>

#include <linux/proc_fs.h>

#include <linux/errno.h>

#include <linux/slab.h>

#include <linux/uaccess.h>

#include <linux/mutex.h>

#include <linux/semaphore.h>
#include <linux/cdev.h>


int scullc_major = SCULLC_MAJOR;
module_param(scullc_major, int, S_IRUGO);

int scullc_devs = SCULLC_DEVS;
module_param(scullc_devs, int, S_IRUGO);

int scullc_qset_size = SCULLC_QSET_SIZE;
module_param(scullc_qset_size, int, S_IRUGO);

int scullc_quantum = SCULLC_QUANTUM;
module_param(scullc_quantum, int, S_IRUGO);

struct scullc_dev *scullc_devps;

kmem_cache_t *global_scullc_cache;


//不能单独调用，必须在file_operations中进行
int scullc_trim(struct scullc_dev *devp)
{
    struct scullc_qset *dptr = devp->qset, next;
    int i;

    if (devp->vmas)
    {
        return -EBUSY;
    }

    for (; dptr ; dptr = next)
    {
        if (dptr->data)
        {
            for (i = 0; i < devp->qset_size; ++i)
            {
                PDEBUG("*(dptr->data + i): %p   dptr->data[i]: %p", *(dapt->data + i), dptr->data[i]);

                if (*(dptr->data + i))
                {
                    kmem_cache_free(global_scullc_cache, *(dptr->data + i));
                }
            }
            kfree(dptr->data);

            next = dptr->next;
            kfree(dptr);
        }
    }

    devp->qset_size = scullc_qset_size;
    devp->quantum = scullc_quantum;
    devp->size = 0;
    devp->qset = NULL;
    return 0;
}

struct scullc_dev *scullc_follow(struct scullc_dev *devp, int n)
{
    struct scullc_qset *dptr = devp->qset, pre_dptr;
    int i;

    
    if (dptr == NULL)
    {
        devp->qset = dptr = kzalloc(sizeof(struct scullc_qset), GFP_KERNEL);
        if (dptr == NULL)
        {
            PDEBUG("no enough memory")
            return NULL;
        }
    }

    for (int i = 0; i < n; ++i)
    {
        if (dptr->next == NULL)
        {
            dptr->next = kzalloc(sizeof(struct scullc_qset), GFP_KERNEL);
            if (dptr->next == NULL)
            {
                PDEBUG("no enough momery");
                return NULL;
            }
        }
        dptr = dptr->next;
    }

    return dptr;
}


static int scullc_open(struct inode *inode, struct file *filp)
{
    struct scullc_dev *devp = container_of(inode->i_cdev, struct scullc_dev, cdev);

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        if (down_interruptible(&devp->sem))
        {
            // return -EINTR wake by signal
            return -ERESTARTSYS;
        }
        scullc_trim(devp);
        up(&devp->sem);
    }
    filp->private_data = devp;
    return 0;
}

static int scullc_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t scullc_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    struct scullc_dev *devp = filp->private_data;
    struct scullc_qset *dptr;
    int item_size;
    int qest_pos, quan_pos, rest;
    int temp;

    ssize_t ret;

    if (down_interruptible(&devp->sem))
    {
        return -ERESTARTSYS;
    }

    if (*pos > devp->size)
    {
        PDEBUG("eof");
        up(&devp->sem);
        return 0;
    }

    item_size = devp->quantum * devp->qset_size;

    if (count > devp->size - *pos)
    {
        count = devp->size - *pos;
    }

    qest_pos = (long)(*pos) / item_size;
    temp = (long)(*pos) % item_size;
    quan_pos =  temp / devp->quantum;
    rest = temp % devp->quantum;

    dptr = scullc_follow(devp, qest_pos);

    if (dptr->data == NULL)
    {
        PDEBUG("not alloc qset memory");
        up(&devp->sem);
        return 0;
    }

    ret = 0;
    do 
    {
        if (*(dptr->data + quan_pos) == NULL)
        {
            PDEBUG("not alloc quantum memory");
            up(&devp->sem);
            return 0;
        }

        if (count > devp->quantum - rest)
        {
            temp = devp->quantum - rest;
        }

        if (copy_to_user(buf, *(dptr->date + quan_pos) + rest, temp))
        {
            up(&devp->sem);
            return -EFAULT;
        }
        rest = 0;
        count -= temp;
        ret += temp;
        *pos += temp;
    } while (count == 0);

    up(&devp->sem);
    return ret;
}

static ssize_t scullc_write(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    struct scullc_dev *devp = filp->private_data;
    struct scullc_qset *dptr = devp->qset;
    int 
}