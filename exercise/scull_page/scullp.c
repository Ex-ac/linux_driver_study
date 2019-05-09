#include "scullp.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/fs.h>
#include <linux/fcntl.h>

#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include <linux/errno.h>

int scullp_major = SCULLP_MAJOR;
module_param(scullp_major, int, S_IRUGO);

int scullp_devices_size = SCULLP_DEVICES_SIZE;
module_param(scullp_devices_size, int, S_IRUGO);

int scullp_order = SCULLP_ORDER;
module_param(scullp_order, int, S_IRUGO);

int scullp_qset_size = SCULLP_QSET_SIZE;
module_param(scullp_qset_size, int, S_IRUGO);


struct scullp_dev *scullp_devps;

int scullp_trim(struct scullp_dev *devp)
{
    int i;
    struct scullp_qset *dptr = devp->qset, *next;

    for (; dptr; dptr = next)
    {
        if (dptr->data)
        {
            for (i = 0; i < devp->qset_size; ++i)
            {
                if (*(dptr->data + i))
                {
                    free_pages((unsigned long)(*(dptr->data + i)), devp->order);
                }
            }
            kfree(dptr->data);
            next = dptr->next;
            kfree(dptr);
        }
    }
    devp->qset_size = scullp_qset_size;
    devp->order = scullp_order;
    devp->size = 0;
    devp->qset = NULL;
    return 0;    
}
#define SCULLP_DIR_WRITE        0x01
#define SCULLP_DIR_READ         0x00

struct scullp_qset *scullp_follow(struct scullp_dev *devp, int index, int dir)
{
    struct scullp_qset *dptr = devp->qset;
    
    if (dptr == NULL)
    {
        if (dir == SCULLP_DIR_READ)
        {
            return NULL;
        }

        devp->qset = dptr = kzalloc(sizeof(struct scullp_qset), GFP_KERNEL);
        if (dptr == NULL)
        {
            return NULL;
        }
    }

    while (index > 0)
    {   
        if (dptr->next == NULL)
        {
            if (dir == SCULLP_DIR_READ)
            {
                return NULL;
            }
            dptr = kzalloc(sizeof(struct scullp_qset), GFP_KERNEL);
            if (dptr->next)
            {
                return NULL;
            }
        }
        
        index --;
        dptr = dptr->next;
    }

    return dptr;
}


static int scullp_open(struct inode *inode, struct file *filp)
{
    struct scullp_dev *devp = container_of(inode->i_cdev, struct scullp_dev, cdev);

    if (filp->f_flags & O_ACCMODE == O_WRONLY)
    {
        if (down_interruptible(&devp->sem))
        {
            return -ERESTARTSYS;
        }
        scullp_trim(devp);
        up(&devp->sem);
    }
    filp->private_data = devp;
    return 0;
}

static int scullp_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t scullp_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    struct scullp_dev *devp = filp->private_data;
    struct scullp_qset *dptr;
    int item_size, qset_pos, order_pos, rest, temp;
    ssize_t ret = 0;

    if (down_interruptible(&devp->sem))
    {
        return -ERESTARTSYS;
    }

    if (*pos > devp->size || *pos < 0)
    {
        up(&devp->sem);
        return 0;
    }

    item_size = devp->qset_size * (PAGE_SIZE << devp->order);
    qset_pos = *pos / item_size;
    temp = *pos % item_size;
    order_pos = temp / (PAGE_SIZE << devp->order);
    rest = temp % (PAGE_SIZE << devp->order);

    dptr = scullp_follow(devp, qset_pos, SCULLP_DIR_READ);

    if (dptr == NULL)
    {
        up(&devp->sem);
        return 0;
    }

    if (*(dptr->data + order_pos) == NULL)
    {
        up(&devp->sem);
        return 0;
    }

    if (count > (PAGE_SIZE << devp->order) - rest)
    {
        count = (PAGE_SIZE << devp->order) - rest;
    }

    if (copy_to_user(buf, *(dptr->data + order_pos) + rest, count))
    {
        ret = -EFAULT;
    }
    else
    {
        ret = count;
        *pos += count;
    }
    
    up(&devp->sem);

    return ret;
}

static ssize_t scullp_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    struct scullp_dev *devp = filp->private_data;
    struct scullp_qset *dptr;
    int temp, qest_pos, order_pos, rest;
    ssize_t ret = 0;

    if (down_interruptible(&devp->sem))
    {
        return -ERESTARTSYS;
    }

    temp = devp->qset_size * (PAGE_SIZE << devp->order);
    qest_pos = *pos / temp;
    temp = *pos % temp;
    order_pos = temp / (PAGE_SIZE << devp->order);
    rest = temp % (PAGE_SIZE << devp->order);

    dptr = scullp_follow(devp, qest_pos, SCULLP_DIR_WRITE);

    if (dptr == NULL)
    {
        up(&devp->sem);
        return -ENOMEM;
    }

    if (dptr->data == NULL)
    {
        dptr->data = kzalloc(sizeof(void *) * devp->qset_size, GFP_KERNEL);
        if (dptr->data == NULL)
        {
            up(&devp->sem);
            return -ENOMEM;
        }
    }

    if (*(dptr->data + order_pos) == NULL)
    {
        *(dptr->data + order_pos) = (void *)(__get_free_pages(GFP_KERNEL, devp->order));

        if (*(dptr->data + order_pos) == NULL)
        {
            up(&devp->sem);
            return -ENOMEM;
        }
        memset(*(dptr->data + order_pos), 0, PAGE_SIZE << devp->order);
    }

    if (count > PAGE_SIZE << devp->order - rest)
    {
        count = PAGE_SIZE << devp->order - rest;
    }

    if (copy_from_user(*(dptr->data + order_pos) + rest, buf, count))
    {
        ret = -EFAULT;
    }
    else
    {
        ret = count;
        *pos += count;

        if (*pos > devp->size)
        {
            devp->size = *pos;
        }
    }
    up(&devp->sem);
    return ret;
}

static loff_t scullp_lseek(struct file *filp, loff_t offset, int whence)
{
    struct scullp_dev *devp = filp->private_data;
    loff_t ret;

    switch (whence)
    {
        case 0:
            ret = offset;
            break;
        case 1:
            ret = filp->f_pos + offset;
            break;
        
        case 2:
            ret = devp->size + offset;
            break;
        
        default:
            return -EINVAL;
    }

    if (ret < 0)
    {
        return -EINVAL;
    }
    filp->f_pos = ret;
    return ret;
}

static long scullp_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
    int ret = 0, temp;
    if (_IOC_TYPE(cmd) != SCULLP_IOC_MAGIC || _IOC_NR(cmd) > SCULLP_IOC_MAX)
    {
        return -EINVAL;
    }

    if (_IOC_DIR(cmd) == _IOC_READ && !access_ok(VERIFY_WRITE, (void __user *)(args), _IOC_SIZE(cmd)))
    {
        return -EINVAL;
    }
    if (_IOC_DIR(cmd) == _IOC_WRITE && !access_ok(VERIFY_READ, (void __user *)(args), _IOC_SIZE(cmd)))
    {
        return -EINVAL;
    }

    switch (cmd)
    {
        case SCULLP_IOC_RESET:
            scullp_qset_size = SCULLP_QSET_SIZE;
            scullp_order = SCULLP_ORDER;
            break;

        case SCULLP_IOC_SET_QSET:
            ret = __get_user(scullp_qset_size, (int __user *)(args));
            break;
        
        case SCULLP_IOC_SET_ORDER:
            ret = __get_user(scullp_order, (int __user *)(args));
            break;

        case SCULLP_IOC_TELL_QSET:
            scullp_qset_size = args;
            break;

        case SCULLP_IOC_TELL_ORDER:
            scullp_order = args;
            break;

        case SCULLP_IOC_GET_QSET:
            ret = __put_user(scullp_qset_size, (int __user *)(args));
            break;

        case SCULLP_IOC_GET_ORDER:
            ret = __put_user(scullp_order, (int __user *)(args));
            break;

        case SCULLP_IOC_QUE_QSET:
            ret = scullp_qset_size;
            break;
        
        case SCULLP_IOC_QUE_ORDER:
            ret = scullp_order;
            break;

        case SCULLP_IOC_EX_QSET:
            temp = scullp_qset_size;
            ret = __get_user(scullp_qset_size, (int __user *)(args));
            if (ret == 0)
            {
                ret = __put_user(temp, (int __user *)(args));
            }
            break;

        case SCULLP_IOC_EX_ORDER:
            temp = scullp_order;
            ret = __get_user(scullp_order, (int __user *)(args));
            if (ret == 0)
            {
                ret = __put_user(temp, (int __user *)(args));
            }
            break;

        case SCULLP_IOC_SHI_QSET:
            ret = scullp_qset_size;
            scullp_qset_size = args;
            break;

        case SCULLP_IOC_SHI_ORDER:
            ret = scullp_order;
            scullp_order = args;
            break;

        default:
            return -EINVAL;
    }

    return ret;
}



static const struct file_operations scullp_fops = 
{
    .owner = THIS_MODULE,
    .open = scullp_open,
    .release = scullp_release,
    .read = scullp_read,
    .write = scullp_write,
    .llseek = scullp_lseek,
    .unlocked_ioctl = scullp_ioctl,
};

#ifdef SCULLP_USE_PROC

static void *scullp_seq_start(struct seq_file *sfilp, loff_t *pos)
{
    seq_printf(sfilp, "scullp proc start\n");
    if (*pos < 0 || *pos >= scullp_devices_size)
    {
        return NULL;
    }
    return scullp_devps + *pos;
}

static void *scullp_seq_next(struct seq_file *sfilp, void *data, loff_t *pos)
{
    seq_printf(sfilp, "scullp proc next\n");
    *pos ++;

    if (*pos < 0 || *pos >= scullp_devices_size)
    {
        return NULL;
    }
    return scullp_devps + *pos;
}

static void scullp_seq_stop(struct seq_file *sfilp, void *data)
{
    seq_printf(sfilp, "scullp proc read  end\n");
}

static int scullp_seq_show(struct seq_file *sfilp, void *data)
{
    struct scullp_dev *devp = (struct scullp_dev *)(data);
    struct scullp_qset *dptr= devp->qset;
    int i;
    char str[81] = {0,0};

    if (down_interruptible(&devp->sem))
    {
        return -ERESTARTSYS;
    }

    seq_printf(sfilp, "Device %li, size %ld, qset size %d, order %d\n", devp - scullp_devps, devp->size, devp->qset_size, devp->order);
    memset(str, '*', 80);
    seq_printf(sfilp, "Detail:\n");
    while (dptr)
    {
        seq_printf(sfilp, "\tqset at %p\n", dptr);
        for (i = 0; i < devp->qset_size; ++i)
        {
            if (*(dptr->data + i))
            {
                seq_printf(sfilp, "\t\t%d: start at %p\n", i, *(dptr->data + i));
                seq_printf(sfilp, "%s\n", str);
                seq_printf(sfilp, "%s\n", *(dptr->data + i));
            }
        }
        dptr = dptr->next;
    }
    // up(&devp->sem);
    up(&devp->sem);
    return 0;
}

static const struct seq_operations scullp_seq_fops = 
{
    .start = scullp_seq_start,
    .next = scullp_seq_next,
    .stop = scullp_seq_stop,
    .show = scullp_seq_show,
};

static int scullp_proc_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &scullp_seq_fops);
}

static const struct file_operations scullp_proc_fops =
{
    .owner = THIS_MODULE,
    .open = scullp_proc_open,
    .release = seq_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static void scullp_proc_create(void)
{
    struct proc_dir_entry *entry = proc_create("scullp", 0, NULL, &scullp_proc_fops);
    if (entry)
    {
        PDEBUG("can't create proc");
    }
}

static void scullp_proc_remove(void)
{
    remove_proc_entry("scullp", NULL);
}

#endif

static void scullp_setup_init(struct scullp_dev *devp, int index)
{
    dev_t devno = MKDEV(scullp_major, index);

    sema_init(&devp->sem, 1);
    devp->order = scullp_order;
    devp->qset_size = scullp_qset_size;
    cdev_init(&devp->cdev, &scullp_fops);

    if (cdev_add(&devp->cdev, devno, 1))
    {
        PDEBUG("can't add device, index is %ld", index);
    }

}

static int __init scullp_init(void)
{
    dev_t devno = MKDEV(scullp_major, 0);
    int ret = 0, i = 0;

    if (scullp_major)
    {
        ret = register_chrdev_region(devno, scullp_devices_size, "scullp");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, scullp_devices_size, "scullp");
        scullp_major = MAJOR(devno);
    }

    if (ret < 0)
    {
        PDEBUG("can't register char device number");
        return ret;
    }

    scullp_devps = kzalloc(sizeof(struct scullp_dev) * scullp_devices_size, GFP_KERNEL);

    if (scullp_devps == NULL)
    {
        unregister_chrdev_region(devno, scullp_devices_size);
        return -ENOMEM;
    }

    for (; i < scullp_devices_size; ++i)
    {
        scullp_setup_init(scullp_devps + i, i);
    }

#ifdef SCULLP_USE_PROC
    scullp_proc_create();
#endif
    return 0;
}

module_init(scullp_init);

static void __exit scullp_exit(void)
{
    int i = 0;

#ifdef SCULLP_USE_PROC
    scullp_proc_remove();
#endif

    for (; i < scullp_devices_size; ++i)
    {
        cdev_del(&(scullp_devps + i)->cdev);
        scullp_trim(scullp_devps + i);
    }

    kfree(scullp_devps);
    unregister_chrdev_region(MKDEV(scullp_major, 0), scullp_devices_size);
}

module_exit(scullp_exit);

MODULE_LICENSE("GPL v2");