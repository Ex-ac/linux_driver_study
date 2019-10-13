#include <fcntl.h>
#include <stdio.h>

#include <sys/ioctl.h>


int main()
{
    int file;

    int quantum = 1100, qset = 200;
    int temp;
    file = open("/dev/scull_0", O_RDONLY | O_NONBLOCK);
    if (file != -1)
    {
        if (temp = ioctl(file, 2147773189, &quantum))
        {
            printf("ioctl error, %d\n", temp);
        }
        ioctl(file, 2147773190, &qset);

        printf("quantum: %d, qset %d\n", quantum, qset);
    }
    else
    {
        printf("can't open file\n");
    }
    

    return 0;
}