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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_size = SCULL_SIZE;
int scull_quantum = SCULL_QUANTUM;
int scull_qset_size = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_size, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset_size, int, S_IRUGO);

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
    devp->qset = scull_qset_size;
    devp->data = NULL;
    return 0;
}




#ifdef SCULL_DEBUG

static int scull_read_procmem(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int i, j, len = 0;

    int limit = count - 80; // 防止写入的内容超出一页

    for (i = 0; i < scull_size && len <= limit; ++i)
    {
        struct scull_dev *devp = scull_devp + i;
        struct scull_qset *dptr = devp->data;
        if (down_interruptible(&devp->semaphore))
        {
            return -ERESTARTSYS;
        }
        len += sprintf(buf + len, "\nDevice %i: qeset %i, quantum %i, size %li\n", i, devp->qset, devp->quantum, devp->size);

        for (; dptr && len <= limit; dptr = dptr->next)
        {
            len += sprintf(buf + len, " item at %p, qset at %p\n", dptr, dptr->data);

            if (dptr->data && !dptr->next)
            {
                for (j = 0; j < devp->qset; ++i)
                {
                    if (dptr->data + j)
                    {
                        len += sprintf(buf + len, "    % 4i: %8p\n", j, *(dptr->data + j));
                    }
                }
            }
        }
        up(&devp->semaphore);
    }
    *eof = 1;
    return len;
}

static void *scull_seq_start(struct seq_file *sfilp, loff_t *pos)
{
    if (*pos >= scull_size)
    {
        return NULL;
    }
    return scull_devp + *pos;
}


static void *scull_seq_next(struct seq_file *sfilp, void *v, loff_t *pos)
{
    *pos ++;
    if (*pos + 1 > scull_size)
    {
        return NULL;
    }
    
    return scull_devp + *pos;
}

static void scull_seq_stop(struct seq_file *sfilp, void *v)
{

}

static int scull_seq_show(struct seq_file *sfilp, void *v)
{
    struct scull_dev *devp = (struct scull_dev *)(v);
    struct scull_qset *dptr;
    int i;

    if (down_interruptible(&devp->semaphore))
    {
        return -ERESTARTSYS;
    }
    seq_printf(sfilp, "\nDevice %i: qset %i, quantum %i, size %li\n", (int)(scull_devp - devp), devp->qset, devp->quantum, devp->size);

    for (dptr = devp->data; dptr; dptr = dptr->next)
    {
        seq_printf(sfilp, "    item at %p, qset at %p\n", dptr, dptr->data);
        if (dptr->data && dptr->next == NULL)
        {
            for (i = 0; i < devp->qset; ++i)
            {
                if (dptr->data[i])
                {
                    seq_printf(sfilp, "        % 4i: %8p\n", i, dptr->data[i]);
                }
            }
        }
    }
    up(&devp->semaphore);
    return 0;
}

struct seq_operations scull_seq_ops = 
{
    .start = scull_seq_start,
    .next = scull_seq_next,
    .stop = scull_seq_stop,
    .show = scull_seq_show,
};


static int scull_proc_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &scull_seq_ops);
}

static struct file_operations scull_proc_ops = 
{
    .owner = THIS_MODULE,
    .open = scull_proc_open,
    .release = seq_release,
    .read = seq_read,
    .llseek = seq_lseek,
};


static void scull_create_proc(void)
{
    struct proc_dir_entry *entry;
    entry = proc_create("scull_seq", 0, NULL, &scull_proc_ops);
    if (entry == NULL)
    {
        printk(KERN_WARNING "scull: create proc faild");
    }
    else
    {
        printk(KERN_WARNING "scull: create proc successed");
    }
    
}

static void scull_remove_proc(void)
{
    remove_proc_entry("scull_seq", NULL);
}
#endif

int scull_open(struct inode *inode, struct file *flip)
{
    struct scull_dev *devp;
    devp = container_of(inode->i_cdev, struct scull_dev, cdev);
    flip->private_data = devp;

    if (flip->f_flags & O_ACCMODE == O_WRONLY)
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

static struct scull_qset *scull_follow(struct scull_dev *devp, int n)
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

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t * f_pos)
{
    struct scull_dev *devp = filp->private_data;
    struct scull_qset *dptr;
    int quantum = devp->quantum, qset = devp->qset;
    int item_size = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t ret = 0;

    if (down_interruptible(&devp->semaphore))
    {
        return -ERESTARTSYS;
    }
    if (*f_pos >= devp->size)
    {
        up(&devp->semaphore);
        return 0;
    }

    item = (long)(*f_pos) / item_size;
    rest = (long)(*f_pos) % item_size;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    dptr = scull_follow(devp, item);

    if (dptr == NULL || dptr->data == NULL || dptr->data[s_pos] == NULL)
    {
        up(&devp->semaphore);
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

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t * f_pos)
{
    struct scull_dev *devp = filp->private_data;
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


int scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

    int ret = 0, temp;
    int err = 0;


    if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC)
    {
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > SCULL_IOC_MAX)
    {
        return -ENOTTY;
    }


    if (_IOC_DIR(cmd) == _IOC_READ)
    {
        if(!access_ok(VERIFY_READ, (void __user *)(arg), _IOC_SIZE(cmd)))
        {
            return -EFAULT;
        }
    }
    if (_IOC_DIR(cmd) == _IOC_WRITE)
    {
        if (!access_ok(VERIFY_WRITE, (void __user *)(arg), _IOC_SIZE(cmd)))
        {
            return -EFAULT;
        }
    }


    switch (cmd)
    {
    case SCULL_IOC_RESET:
        scull_quantum = SCULL_QUANTUM;
        scull_qset_size = SCULL_QSET;
        break;

    case SCULL_IOC_SQUANTUM:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        ret = __get_user(scull_quantum, (int __user *)(arg));
        break;
    
    case SCULL_IOC_SQSET:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        ret = __get_user(scull_qset_size, (int __user *)(arg));
        break;

    case SCULL_IOC_TQUANTUM:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        scull_quantum = arg;
        break;
    
    case SCULL_IOC_TQSET:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        scull_qset_size = arg;
        break;

    case SCULL_IOC_GQUANTUM:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        ret = __put_user(scull_quantum, (int __user *)(arg));
        break;

    case SCULL_IOC_GQSET:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        ret = __put_user(scull_qset_size, (int __user *)(arg));
        break;

    case SCULL_IOC_QQUANTUM:
        return scull_quantum;

    case SCULL_IOC_QQSET:
        return scull_qset_size;

    case SCULL_IOC_XQUANTUM:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        temp = scull_quantum;
        ret = __get_user(scull_quantum, (int __user *)(arg));
        if (ret == 0)
        {
            ret = __put_user(temp, (int __user *)(arg));
        }
        break;

    case SCULL_IOC_XQSET:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        temp = scull_qset_size;
        ret = __get_user(scull_qset_size, (int __user *)(arg));
        if (ret == 0)
        {
            ret = __put_user(temp, (int __user *)(arg));
        }
        break;

    case SCULL_IOC_HQUANTUM:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        temp = scull_quantum;
        scull_quantum = arg;
        return temp;

    case SCULL_IOC_HQSET:
        if (!capable(CAP_SYS_ADMIN))
        {
            return -EPERM;
        }
        temp = scull_qset_size;
        scull_qset_size = arg;
        return temp;


    default:
        return -ENOTTY;
    }

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

    cdev_init(&devp->cdev, &scull_fops);
    devp->cdev.owner = THIS_MODULE;
    devp->cdev.ops = &scull_fops;
    err = cdev_add(&devp->cdev, devno, 1);

    if (err)
    {
        printk(KERN_NOTICE "scull: %ld adding error, error code: %ld\n", index, err);
    }
    else
    {
        printk(KERN_NOTICE "scull: %ld adding finish\n", index);
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
        scull_devp[i].qset = scull_qset_size;
        sema_init(&scull_devp[i].semaphore, 1);
        scull_setup_cdev(scull_devp + i, i);
    }

    printk(KERN_INFO "scull: quantum: %ld, qset: %ld\n", SCULL_IOC_GQUANTUM, SCULL_IOC_GQSET);

#ifdef SCULL_DEBUG
    scull_create_proc();
#endif

    return 0;
}

module_init(scull_init);

static void __exit scull_exit(void)
{
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);

    printk(KERN_WARNING "scull: start exit\n");
    for (i = 0; i < scull_size; ++i)
    {
        scull_trim(scull_devp + i);
        cdev_del(&(scull_devp + i)->cdev);
        printk(KERN_WARNING "scull: %d del\n", i);
    }
    kfree(scull_devp);
    printk(KERN_WARNING "scull: kfree scull_devp\n");

    printk(KERN_WARNING "scull: unregister\n");
    unregister_chrdev_region(devno, scull_size);

#ifdef SCULL_DEBUG
    printk(KERN_WARNING "scull: remove proc\n");
    scull_remove_proc();
#endif
}

module_exit(scull_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ex-ac ex-ac@outlook.com");
