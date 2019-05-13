#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/cdev.h>

#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/iomap.h>
#include <linux/uaccess.h>
#include <asm-generic/io.h>

static int silly_major = 0;
module_param(silly_major, int, S_IRUGO);

#define ISA_BASE            0xA0000
#define ISA_MAX             0x100000

#define VIDEO_MAX           0xC0000
#define VGA_BASE            0xB8000

struct silly_dev
{
    struct cdev cdev;
    struct resource *silly_resource;
};

struct silly_dev *global_silly_devp;
// virual mem
static void __iomem *io_base;

static int silly_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int silly_release(struct inode *inode, struct file *filp)
{
    return 0;
}

enum silly_mode {M_8 = 0, M_16, M_32, M_memory};

static ssize_t silly_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    int ret;
    int mode = iminor(filp->f_inode);
    void __iomem *add;
    unsigned long isa_addr = ISA_BASE + *pos;
    unsigned char *kbuf, *ptr;

    if (isa_addr + count > ISA_MAX)
    {
        count = ISA_MAX - isa_addr;
    }

    if (count <= 0)
    {
        return 0;
    }

    kbuf = kzalloc(count, GFP_KERNEL);

    if (kbuf == NULL)
    {
        return -ENOMEM;
    }
    ptr = buf;
    ret = count;

    add = (void __iomem *)(io_base + (isa_addr - ISA_BASE));

    if (mode == M_32 && ((isa_addr | count) & 3))
    {
        mode = M_16;
    }
    if (mode == M_16 && ((isa_addr | count) &1))
    {
        mode = M_8;
    }

    switch (mode)
    {
        case M_8:
            while (count)
            {
                *ptr = ioread8(add);
                count --;
                add ++;
                ptr ++;
            }
            break;

        case M_16:
            while (count >= 2)
            {
                *((uint16_t *)(ptr)) = ioread16(add);
                count -= 2;
                add += 2;
                ptr += 2;
            }
            break;

        case M_32:
            while (count >= 4)
            {
                *((uint32_t *)(ptr)) = ioread32(add);
                count -= 4;
                add += 4;
                ptr += 4;
            }
            break;

        case M_memory:
            memcpy_fromio(ptr, add, count);
            break;

        default:
            return -EINVAL;
    }

    if (ret > 0 && copy_to_user(buf, kbuf, count))
    {
        ret = -EFAULT;
    }
    kfree(kbuf);
    *pos += ret;
    return ret;
}

static ssize_t silly_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    int ret;
    int mode = iminor(filp->f_inode);
    unsigned long isa_addr = ISA_BASE + *pos;
    unsigned char *kbuf, *ptr;
    void __iomem *add;

    if (!capable(CAP_SYS_RAWIO))
    {
        return -EPERM;
    }

    if (isa_addr + count > ISA_MAX)
    {
        count = ISA_MAX - isa_addr;
    }

    if (count <= 0)
    {
        return 0;
    }

    kbuf = kzalloc(count, GFP_KERNEL);

    if (kbuf == NULL)
    {
        return -ENOMEM;
    }
    ptr = kbuf;

    if (copy_from_user(kbuf, buf, count))
    {
        kfree(kbuf);
        return -EFAULT;
    }

    ret = count;

    if (mode == M_32 && ((isa_addr | count) & 3))
    {
		mode = M_16;
    }
	if (mode == M_16 && ((isa_addr | count) & 1))
    {
		mode = M_8;
    }

    add = (void __iomem *)(io_base + (isa_addr - ISA_BASE));

    switch (mode)
    {
    case M_32:
        while (count > 4)
        {
            iowrite32(*((uint32_t *)(ptr)), add);
            ptr += 4;
            count -= 4;
            add += 4;
        }
        break;

    case M_16:
        while (count > 2)
        {
            iowrite16(*((uint16_t *)(ptr)), add);
            ptr += 2;
            add += 2;
            count -= 2;
        }
        break;

    case M_8:
        while (count)
        {
            iowrite8(*ptr, add);
            ptr ++;
            add ++;
            count --;
        }
        break;
    case M_memory:
        memcpy_toio(add, ptr, count);
        break;

    default:
        ret =  -EINVAL;
        break;
    }

    if (ret > 0)
    {
        *pos += ret;
    }
    kfree(kbuf);
    return ret;
}

static int silly_poll(struct file *filp, poll_table *wait)
{
    return POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;
}

static const struct file_operations silly_fops = 
{
    .owner = THIS_MODULE,
    .open = silly_open,
    .release = silly_release,
    .read = silly_read,
    .write = silly_write,
};

static int __init silly_init(void)
{
    dev_t devno = MKDEV(silly_major, 0);
    int ret;


    if (silly_major)
    {
        ret = register_chrdev_region(devno, 1, "silly");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "silly");
        silly_major = MAJOR(devno);
    }
     

    if (ret < 0)
    {
        printk(KERN_INFO "silly: can't register char device\n");
        return ret;
    }
    printk(KERN_INFO "silly: register char device\n");

    global_silly_devp = kzalloc(sizeof(struct silly_dev), GFP_KERNEL);

    if (global_silly_devp == NULL)
    {
        unregister_chrdev_region(devno, 1);
        return -ENOMEM;
    }
    printk(KERN_INFO "silly: alloc mem\n");

    // global_silly_devp->silly_resource = request_mem_region(ISA_BASE, ISA_MAX, "silly");
    // if (global_silly_devp->silly_resource == NULL)
    // {
    //     printk(KERN_INFO "silly: can't request mem region\n");
    //     unregister_chrdev_region(devno, 1);
    //     kfree(global_silly_devp);
    //     return -EFAULT;
    // }
    // printk(KERN_INFO "silly: request mem region\n");

    io_base = ioremap(ISA_BASE, ISA_MAX);
    printk(KERN_INFO "silly: ioremap mem\n");

    cdev_init(&global_silly_devp->cdev, &silly_fops);
    global_silly_devp->cdev.owner = THIS_MODULE;
    ret = cdev_add(&global_silly_devp->cdev, devno, 1);

    return ret;
}

module_init(silly_init);


static void __exit silly_exit(void)
{
    cdev_del(&global_silly_devp->cdev);
    unregister_chrdev_region(MKDEV(silly_major, 0), 1);
    iounmap(io_base);
    // release_mem_region(ISA_BASE, ISA_MAX);

    kfree(global_silly_devp);
}

module_exit(silly_exit);

MODULE_LICENSE("GPL v2");