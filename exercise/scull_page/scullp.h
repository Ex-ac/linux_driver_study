#ifndef SCULLP_H_
#define SCULLP_H_

#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/ioctl.h>
#include <linux/semaphore.h>

#define SCULLP_DEBUG

#undef PDEBUG
#ifdef SCULLP_DEBUG
#   ifdef __KERNEL__
#       define PDEBUG(fmt. args...) printk(KERN_INFO "scullp: " fmt, ## args)
#   else
#       define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#   endif
#else
#   define PDEBUG(fmt, args...)
#endif

#define SCULLP_MAJOR            0
#define SCULLP_DEVICES_SIZE     4

#define SCULLP_ORDER            0
#define SCULLP_QSET_SIZE        500

struct scullp_qset
{
    struct scullp_qset *next;
    void **data;
};

struct scullp_dev
{
    struct cdev cdev;
    struct scullp_qset *qset;
    int order;
    int qset_size;
    long size;
    struct semaphore sem;
};


extern int scullp_major;
extern int scullp_devices_size;
extern int scullp_order;
extern int scullp_qset_size;

int scullp_trim(struct scullp_dev *devp);
struct scullp_qset *scullp_follow(struct scullp_dev *devp, int index, int dir);

#ifdef SCULLP_DEBUG
#   define SCULLP_USE_PROC
#endif

#define SCULLP_IOC_MAGIC        'k'

#define SCULLP_IOC_RESET        _IO(SCULLP_IOC_MAGIC, 0)

#define SCULLP_IOC_SET_ORDER    _IOW(SCULLP_IOC_MAGIC, 1, int)
#define SCULLP_IOC_SET_QSET     _IOW(SCULLP_IOC_MAGIC, 2, int)

#define SCULLP_IOC_TELL_ORDER   _IO(SCULLP_IOC_MAGIC, 3)
#define SCULLP_IOC_TELL_QSET    _IO(SCULLP_IOC_MAGIC, 4)

#define SCULLP_IOC_GET_ORDER    _IOR(SCULLP_IOC_MAGIC, 5, int)
#define SCULLP_IOC_GET_QSET     _IOR(SCULLP_IOC_MAGIC, 6, int)

#define SCULLP_IOC_QUE_ORDER    _IO(SCULLP_IOC_MAGIC, 7)
#define SCULLP_IOC_QUE_QSET     _IO(SCULLP_IOC_MAGIC, 8)

#define SCULLP_IOC_EX_ORDER     _IOWR(SCULLP_IOC_MAGIC, 9, int)
#define SCULLP_IOC_EX_QSET      _IOWR(SCULLP_IOC_MAGIC, 10, int)

#define SCULLP_IOC_SHI_ORDER    _IO(SCULLP_IOC_MAGIC, 11)
#define SCULLP_IOC_SHI_QSET     _IO(SCULLP_IOC_MAGIC, 12)

#define SCULLP_IOC_MAX          12

#endif