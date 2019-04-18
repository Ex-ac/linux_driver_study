// #include <stdio.h>
// #include <linux/poll.h>
// #include <linux/kernel.h>
// #include <linux/cdev.h>
// #include <linux/fs.h>
// #include <linux/module.h>
// #include <linux/init.h>
// #include <linux/slab.h>
// #include <linux/uaccess.h>
// #include <linux/errno.h>
// #include <linux/mutex.h>
// #include <linux/wait.h>
// #include <linux/sched.h>
// #include <linux/signal.h>
#include <linux/poll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

// #define GLOBALFIFO_MAGIC 'g'
// #define GLOBALFIFO_CLEAR _IO(GLOBALFIFO_MAGIC, 0)
#define GLOBALFIFO_SIZE 0xff

int main(void)
{
    int fd, num;
    char rd_ch[GLOBALFIFO_SIZE];

    fd_set rfds, wfds;

    fd = open("/dev/globalfifo_0", O_RDONLY | O_NONBLOCK);

    if (fd != -1)
    {
        // if (ioctl(fd, GLOBALFIFO_CLEAR) < 0)
        // {
        //     printf("ioctl command faild\n");
        // }

        while (1)
        {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);

            FD_SET(fd, &rfds);
            FD_SET(fd, &wfds);

            select(fd + 1, &rfds, &wfds, 0, 0);

            if (FD_ISSET(fd, &rfds))
            {
                printf("Poll moniter: can be read\n");
            }

            if (FD_ISSET(fd, &wfds))
            {
                printf("Poll monitor: can be written\n");
            }
            else
            {
                printf("Device open failure\n");
            }
        }

        return 0;
    }
}