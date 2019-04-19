#ifndef SCULL_H_
#define SUCLL_H_

#include <linux/ioctl.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define SCULL_DEBUG

#undef PRINT_DEBUG
#ifdef SCULL_DEBUG
#   ifdef __KERNEL__
#       define PRINT_DEBUG(fmt, args...) printk(KERN_DEBUG "scull: " fmt, ##args)
#   else
#       define PRINT_DEBUG(fmt, args...) printf(KERN_DEBUG "scull: " fmt, ##args)
#   endif
#else
#   define PRINT_DEBUG(fmt, args...)
#endif

#undef PRINT_DEBUGG
#define PRINT_DEBUGG(fmt, args...)

#ifndef SCULL_MAJOR
#define SCULL_MAJOR     0
#endif

#ifndef SCULL_SIZE
#define SCULL_SIZE      4
#endif


#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM   4096
#endif

#ifndef SCULL_QSET
#define SCULL_QSET      1000
#endif

struct scull_qset
{
    void **data;
    struct scull_qset *next;
};

struct scull_dev
{
    struct scull_qset *data;
    int quantum;
    int qset;
    unsigned long size;
    unsigned int access_key;
    struct semaphore semaphore;
    struct cdev cdev;
};

extern int scull_major;
extern int scull_size;
extern int scull_quantum;
extern int scull_qset;


int scull_trim(struct scull_dev *devp);

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

loff_t scull_llseek(struct file *filp, loff_t offset, int whence);

#define SCULL_IOC_MAGIC     'k'
#define SCULL_IOC_RESET     _IO(SCULL_IOC_MAGIC, 0)




#endif
