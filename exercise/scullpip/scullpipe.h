#ifndef SCULLPIPE_H_
#define SCULLPIPE_H_

#include <linux/kernel.h>
#include <linux/ioctl.h>

#undef PRINT_DEBUG
#ifndef SCULLPIPE_DEBUG
#   ifdef __KERNEL__
#       define PRINT_DEBUG(fmt, args...) printk(KERN_INFO "scull pipe: " fmt, ## args)
#   else
#       define PRINT_DEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#   endif
#else
#   define PRINT_DEBUG(fmt, args...)
#endif

#define SCULLPIPE_MAJOR     0
#define SCULLPIPE_MINOR     0
#define SCULLPIPE_SIZE      1

#define SCULLPIPE_BUFF_SIZE 1024


extern int scullpipe_size;
extern int scullpipe_major;
extern int scullpipe_mimor;



#define SCULLPIPE_MAGIC     'p'

#define SCULLPIPE_IOC_RESET _IO(SCULLPIPE_MAGIC, 0)


#endif
