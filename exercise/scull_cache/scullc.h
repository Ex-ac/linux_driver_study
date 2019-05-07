#ifndef SCULLC_H_
#define SCULLC_H_

#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>



#undef PDEBUG
#ifdef SCULLC_DEBUG
#   ifdef __KERNEL__
#       define PDEBUG(fmt, args...) printk(KERN_DEBUG "scullc: " fmt, ## args)
#   else
#       define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#   endif 
#else
#define PDEBUG(fmt, args...)
#endif

#define SCULLC_MAJOR    0
#define SCULLC_DEVS     4


#define SCULLC_QUANTUM  4096
#define SCULLC_QSET_SIZE 500

struct scullc_qset
{
    void **data;
    struct scullc_qset *next;
};


struct scullc_dev
{
    struct_qset *qset;    
    int vmas;
    int quantum;
    int qset_size;
    size_t size;
    struct semaphore sem;
    struct cdev cdev;
};



extern struct scullc_dev *scullc_devps;
extern const struct file_operations scullc_fops;

extern int scullc_major;
extern int scullc_devs;
extern int scullc_quantum;
extern int scullc_qset_size;

int scullc_trim(struct scullc_dev *devp);
struct scullc_qset *scullc_follow(struct scullc_dev *devp, int n);

#ifdef SCULLC_DEBUG
#   define SCULLC_USE_PROC
#endif

#define SCULLC_IOC_MAGIC        'K'

#define SCULLC_IOC_RESET        _IO(SCULLC_IOC_MAGIC, 0)

#define SCULLC_IOC_SET_QUANTUM      _IOW(SCULLC_IOC_MAGIC, 1, int)
#define SCULLC_IOC_SET_QSET         _IOW(SCULLC_IOC_MAGIC, 2, int)
#define SCULLC_IOC_TELL_QUANTUM     _IO(SCULLC_IOC_MAGIC, 3)
#define SCULLC_IOC_TELL_QSET        _IO(SCULLC_IOC_MAGIC, 4)
#define SCULLC_IOC_GET_QUANTUM      _IOR(SCULLC_IOC_MAGIC, 5, int)
#define SCULLC_IOC_GET_QSET         _IOR(SCULLC_IOC_MAGIC, 6, int)
#define SCULLC_IOC_QUE_QUANTUM      _IO(SCULLC_IOC_MAGIC, 7)
#define SCULLC_IOC_QUE_QSET         _IO(SCULLC_IOC_MAGIC, 8)
#define SCULLC_IOC_EX_QUANTUM       _IOWR(SCULLC_IOC_MAGIC, 9, int)
#define SCULLC_IOC_EX_QSERT         _IOWR(SCULLC_IOC_MAGIC, 10, int)
#define SCULLC_IOC_SHIFT_QUANTUM    _IO(SCULLC_IOC_MAGIC, 11)
#define SCULLC_IOC_SHIFT_QSET       _IO(SCULLC_IOC_MAGIC, 12)

#define SCULLC_IOC_MAX              12