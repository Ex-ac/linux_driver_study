#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#define MAX_LEN     100


void input_handler(int num)
{
    printf("globalfifo: Receive a signal form globalfifo, signalnum: %d\n", num);
} 

int main(void)
{
    int oflages;
    int fd;

    fd = open("/dev/globalfifo_0", O_RDWR, S_IRUSR | S_IWUSR);

    if (fd != -1)
    {
        signal(SIGIO, input_handler);
        fcntl(fd, F_SETOWN, getpid());
        oflages = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, oflages | FASYNC);

        while (1)
        {
            sleep(100);
        }
    }
    else
    {
        printf("device open failure\n");
    }
    
    return 0;
}