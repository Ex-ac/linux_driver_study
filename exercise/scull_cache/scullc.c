#include "scullc.h"

#include <linux/init.h>

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/fs.h>

#include <linux/fcntl.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

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

struct  kmem_cache *global_scullc_cache;


//不能单独调用，必须在file_operations中进行
int scullc_trim(struct scullc_dev *devp)
{
    struct scullc_qset *dptr = devp->qset, *next;
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
                PDEBUG("*(dptr->data + i): %p   dptr->data[i]: %p", *(dptr->data + i), dptr->data[i]);

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

struct scullc_qset *scullc_follow(struct scullc_dev *devp, int n)
{
    struct scullc_qset *dptr = devp->qset, pre_dptr;
    int i;

    
    if (dptr == NULL)
    {
        devp->qset = dptr = kzalloc(sizeof(struct scullc_qset), GFP_KERNEL);
        if (dptr == NULL)
        {
            PDEBUG("no enough memory");
            return NULL;
        }
        dptr->data = kzalloc(sizeof(void *) * scullc_quantum, GFP_KERNEL);
    }

    for (i = 0; i < n; ++i)
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
        // if (down_interruptible(&devp->sem))
        // {
        //     // return -EINTR wake by signal
        //     return -ERESTARTSYS;
        // }
        if (mutex_lock_interruptible(&devp->mutex))
        {
            return -ERESTARTSYS;
        }
        scullc_trim(devp);
        // mutex_unlock(&devp->mutex);
        mutex_unlock(&devp->mutex);
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

    // if (down_interruptible(&devp->sem))
    // {
    //     return -ERESTARTSYS;
    // }
    if (mutex_lock_interruptible(&devp->mutex))
    {
        return -ERESTART;
    }

    if (*pos > devp->size)
    {
        PDEBUG("eof");
        // mutex_unlock(&devp->mutex);
        mutex_unlock(&devp->mutex);
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
        // mutex_unlock(&devp->mutex);
        mutex_unlock(&devp->mutex);
        return 0;
    }

    ret = 0;
    do 
    {
        if (*(dptr->data + quan_pos) == NULL)
        {
            PDEBUG("not alloc quantum memory");
            // mutex_unlock(&devp->mutex);
            mutex_unlock(&devp->mutex);
            return 0;
        }

        if (count > devp->quantum - rest)
        {
            temp = devp->quantum - rest;
        }
        else
        {
            temp = count;
        }
        

        if (copy_to_user(buf, *(dptr->data + quan_pos) + rest, temp))
        {
            // mutex_unlock(&devp->mutex);
            mutex_unlock(&devp->mutex);
            return -EFAULT;
        }
        rest = 0;
        count -= temp;
        ret += temp;
        buf += temp;
    } while (count);

    *pos += ret;

    // mutex_unlock(&devp->mutex);
    mutex_unlock(&devp->mutex);
    return ret;
}

static ssize_t scullc_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    struct scullc_dev *devp = filp->private_data;
    struct scullc_qset *dptr;
    int item_size;
    int temp, qset_pos, quan_pos, rest;
    ssize_t ret = 0;
    // if (down_interruptible(&devp->sem))
    // {
    //     return -ERESTARTSYS;
    // }
    if (mutex_lock_interruptible(&devp->mutex))
    {
        return -ERESTARTSYS;
    }
    item_size = devp->quantum * devp->qset_size;

    qset_pos = (long)(*pos) / item_size;
    temp = (long)(*pos) % item_size;
    quan_pos = temp / devp->quantum;
    rest = temp % devp->quantum;

    dptr = scullc_follow(devp, qset_pos);

    if (dptr == NULL)
    {
        PDEBUG("can't alloc qset");
        // mutex_unlock(&devp->mutex);
        mutex_unlock(&devp->mutex);
        return ret;
    }

    if (dptr->data == NULL)
    {
        dptr->data = kzalloc(sizeof(void *) * devp->qset_size, GFP_KERNEL);

        if (dptr->data == NULL)
        {
            return -ENOMEM;
        }
    }
    do
    {
        if (count > devp->quantum - rest)
        {
            temp = devp->quantum - rest;
        }
        else
        {
            temp = count;
        }
        
        if (*(dptr->data + quan_pos) == NULL)
        {
            *(dptr->data + quan_pos) = kmem_cache_alloc(global_scullc_cache, GFP_KERNEL);
            if (*(dptr->data + quan_pos) == NULL)
            {
                PDEBUG("can't alloc memery from cache");
                // mutex_unlock(&devp->mutex);
                mutex_unlock(&devp->mutex);
                return -ENOMEM;
            }
        }
        
        if (copy_from_user(*(dptr->data += quan_pos) + rest, buf, temp))
        {
            mutex_unlock(&devp->mutex);
            return -EFAULT;
        }

        rest = 0;
        buf += temp;
        ret += temp;
        count -= temp;
    } while (count);
    
    *pos += ret;

    if (*pos > devp->size)
    {
        devp->size = *pos;
    }
    mutex_unlock(&devp->mutex);
    return ret;
}

// 位置只有在读取时有作用，该地方做一个简单的加减就行，我们的数据是可变长度的
static loff_t scullc_lseek(struct file *filp, loff_t off, int whence)
{
    struct scullc_dev *devp = filp->private_data;
    loff_t ret;

    switch (whence)
    {
    case 0:
        ret = off;
        break;

    case 1:
        ret = (loff_t)(filp->f_pos) + off;
        break;

    case 2:
        ret = (loff_t)(devp->size) + off;
        break;

    default:
        ret = -EINVAL;
    }

    if (ret < 0)
    {
        return -EINVAL;
    }

    filp->f_pos = ret;

    return ret;
}

static long scullc_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
    // struct scullc_dev *devp = (struct scullc_dev *)(filp->private_data);
    int ret = 0, temp;

    if (_IOC_TYPE(cmd) != SCULLC_IOC_MAGIC)
    {
        return -ENOTTY;
    }

    if (_IOC_NR(cmd) > SCULLC_IOC_MAX)
    {
        return -ENOTTY;
    }

    if (_IOC_DIR(cmd) & _IOC_READ && !access_ok(VERIFY_WRITE, (void * __user)(args), _IOC_SIZE(cmd)))
    {
        return -EFAULT;
    }
    if (_IOC_DIR(cmd) & _IOC_WRITE && !access_ok(VERIFY_READ, (void * __user)(args), _IOC_SIZE(cmd)))
    {
        return -EFAULT;
    }

    switch (cmd)
    {
        case SCULLC_IOC_RESET:
            scullc_qset_size = SCULLC_QSET_SIZE;
            scullc_quantum = SCULLC_QUANTUM;
            break;

        case SCULLC_IOC_SET_QSET:
            ret = __get_user(scullc_qset_size, (int __user *)(args));
            break;

        case SCULLC_IOC_SET_QUANTUM:
            ret = __get_user(scullc_quantum, (int __user *)(args));
            break;
        
        case SCULLC_IOC_TELL_QSET:
            scullc_qset_size = args;
            break;

        case SCULLC_IOC_TELL_QUANTUM:
            scullc_quantum = args;
            break;
        
        case SCULLC_IOC_QUE_QSET:
            ret = __put_user(scullc_qset_size, (int __user *)(args));
            break;

        case SCULLC_IOC_QUE_QUANTUM:
            ret = __put_user(scullc_quantum, (int __user *)(args));
            break;

        case SCULLC_IOC_EX_QSERT:
            temp = scullc_qset_size;
            ret = __get_user(scullc_qset_size, (int __user *)(args));
            if (ret == 0)
            {
                ret = __put_user(temp, (int __user *)(args));
            }
            break;

        case SCULLC_IOC_EX_QUANTUM:
            temp = scullc_quantum;
            ret = __get_user(scullc_quantum, (int __user *)(args));
            if (ret == 0)
            {
                ret = __put_user(temp, (int __user *)(args));
            }
            break;
        
        case SCULLC_IOC_SHIFT_QSET:
            ret = scullc_qset_size;
            scullc_qset_size = args;
            break;

        case SCULLC_IOC_SHIFT_QUANTUM:
            ret = scullc_quantum;
            scullc_quantum = args;
            break;

        default :
            ret = -ENOTTY;
    }  

    return ret;
}

static const struct file_operations scullc_fops = 
{
    .owner = THIS_MODULE,
    .open = scullc_open,
    .release = scullc_release,
    .read = scullc_read,
    .write = scullc_write,
    .llseek = scullc_lseek,
    .unlocked_ioctl = scullc_ioctl,
};

#define SCULLC_USE_PROC

#ifdef SCULLC_USE_PROC

static void *scullc_seq_start(struct seq_file *sfilp, loff_t *pos)
{
    if (*pos >= scullc_devs || *pos < 0)
    {
        return NULL;
    }
    return scullc_devps + *pos;
}

static void *scullc_seq_next(struct seq_file *sfilp, void *data, loff_t *pos)
{
    (*pos)++;
    if (*pos >= scullc_devs || *pos < 0)
    {
        return NULL;
    }
    return scullc_devps + *pos;
}
static void scullc_seq_stop(struct seq_file *sfilp, void *data)
{
    seq_printf(sfilp, "scullc proc read end\n");
}

static int scullc_seq_show(struct seq_file *sfilp, void *data)
{
    struct scullc_dev *devp = (struct scullc_dev *)(data);
    struct scullc_qset *dptr = devp->qset;
    int i;

    // down(&devp->sem);
    if (mutex_lock_interruptible(&devp->mutex))
    {
        return -ERESTARTSYS;
    }

    seq_printf(sfilp, "Device %li, size %ld, qset size %d, quantum %d\n", devp - scullc_devps, devp->size, devp->qset_size, devp->quantum);
    
    seq_printf(sfilp, "Detail:\n");
    while (dptr)
    {
        seq_printf(sfilp, "\tqset at %p\n", dptr);
        for (i = 0; i < devp->qset_size; ++i)
        {
            if (*(dptr->data + i))
            {
                seq_printf(sfilp, "\t\t%d: start at %p\n", i, *(dptr->data + i));
                seq_printf(sfilp, "\t\t%s\n", *(dptr->data) + i);
            }
        }
        dptr = dptr->next;
    }
    // mutex_unlock(&devp->mutex);
    mutex_unlock(&devp->mutex);
    return 0;
}


static const struct seq_operations scullc_seq_ops = 
{
    .start = scullc_seq_start,
    .next = scullc_seq_next,
    .stop = scullc_seq_stop,
    .show = scullc_seq_show,
};

static int scullc_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &scullc_seq_ops);
}

static const struct file_operations seq_ops = 
{
    .owner = THIS_MODULE,
    .open = scullc_seq_open,
    .release = seq_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static void scullc_proc_create(void)
{
    struct proc_dir_entry *entry;
    entry = proc_create("scullc", 0, NULL, &seq_ops);
    if (entry)
    {
        PDEBUG("create proc success");
    }
}

static void scullc_proc_remove(void)
{
    remove_proc_entry("scullc", NULL);
}
#endif




static void scullc_setup_cdev(struct scullc_dev *devp, int index)
{
    int err;
    dev_t devno = MKDEV(scullc_major, index);

    sema_init(&devp->sem, 1);
    mutex_init(&devp->mutex);

    cdev_init(&devp->cdev, &scullc_fops);
    devp->cdev.owner = THIS_MODULE;
    
    devp->quantum = scullc_quantum;
    devp->qset_size = scullc_qset_size;

    err = cdev_add(&devp->cdev, devno, 1);

    if (err)
    {
        PDEBUG("can't add device NO: %i", index);
    }
}

static void __exit scullc_exit(void)
{
    int i = 0;

#ifdef SCULLC_USE_PROC
    scullc_proc_remove();
#endif

    for (; i < scullc_devs; ++i)
    {
        cdev_del(&(scullc_devps + i)->cdev);
        scullc_trim(scullc_devps + i);
    }

    kfree(scullc_devps);

    if (global_scullc_cache)
    {
        kmem_cache_destroy(global_scullc_cache);
    }
    unregister_chrdev_region(MKDEV(scullc_major, 0), scullc_devs);
}

module_exit(scullc_exit);

static int __init scullc_init(void)
{
    int ret, i = 0;
    dev_t devno = MKDEV(scullc_major, 0);

    if (scullc_major)
    {
        ret = register_chrdev_region(devno, scullc_devs, "scullc");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, scullc_devs, "scullc");
        scullc_major = MAJOR(devno);
    }

    if (ret < 0)
    {
        PDEBUG("can't register cdev number");
        return ret;
    }

    scullc_devps = kzalloc(sizeof(struct scullc_dev) * scullc_devs, GFP_KERNEL);

    if (scullc_devps == NULL)
    {
        unregister_chrdev_region(devno, scullc_devs);

        return -ENOMEM;
    }

    for (; i < scullc_devs; ++i)
    {
        scullc_setup_cdev(scullc_devps + i, i);
    }

    global_scullc_cache = kmem_cache_create("scullc", scullc_quantum, 0, SLAB_HWCACHE_ALIGN, NULL);

    if (global_scullc_cache == NULL)
    {
        scullc_exit();
        return -ENOMEM;
    }

#ifdef SCULLC_USE_PROC
    scullc_proc_create();
#endif
    
    return 0;
}

module_init(scullc_init);

MODULE_LICENSE("GPL v2");