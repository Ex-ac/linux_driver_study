#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include <libaio.h>

#define BUFF_SIZE 4096

int main(int argc, char **argv)
{
    io_context_t ctx = 0;
    struct iocb cb;
    struct iocb *cbs[1];

    unsigned char *buff;

    struct io_event events[1];

    int ret;
    int fd;

    if (argc < 2)
    {
        printf("The command format : aior [FILE]\n");
        exit(1);
    }

    fd = open(argv[1], O_RDWR | O_DIRECT);

    if (fd < 0)
    {
        printf("open error\n");
        return -1;
    }

    // 用于内部的内存对齐
    ret = posix_memalign((void **)(&buff), 512, (BUFF_SIZE + 1));
    if (ret < 0)
    {
        printf("posix_memalign failed\n");
        close(fd);
        return -1;
    }

    memset(buff, 0, BUFF_SIZE + 1);

    // 初始化 io_contex_t，主要是设置最大的事件数
    ret = io_setup(128, &ctx);
    if (ret < 0)
    {
        printf("io_setup error: %s\n", strerror(-ret));
        free(buff);
        close(fd);
        return -1;
    }
    // 用于控制aio的控制数据块
    io_prep_pread(&cb, fd, buff, BUFF_SIZE, 0);

    cbs[0] = &cb;

    // 启动任务
    ret = io_submit(ctx, 1, cbs);

    if (ret != 1)
    {
        if (ret < 0)
        {
            printf("io_sumbit error: %s\n", strerror(-ret));
        }
        else
        {
            {
                fprintf(stderr, "could not sumbit IOs\n");
            }

            if ((ret == io_destroy(ctx)) < 0)
            {
                printf("io_destroy error: %s\n", strerror(-ret));
            }
            free(buff);
            close(fd);
            return -1;
        }
    }


    // 阻塞
    ret = io_getevents(ctx, 1, 1, events, NULL);

    if (ret != 1)
    {
        if (ret < 0)
        {
            printf("io_getevents error: %s", strerror(-ret));
        }
        else
        {
            fprintf(stderr, "counld not get Events\n");
        }

        if ((ret == io_destroy(ctx)) < 0)
        {
            printf("io_destroy error: %s\n", strerror(-ret));
        }
        free(buff);
        close(fd);
        return -1;
    }

    if (events[0].res2 == 0)
    {
        printf("%s\n", buff);
    }
    else
    {
        printf("AIO error: %s\n", strerror(-events[0].res));

        if ((ret == io_destroy(ctx)) < 0)
        {
            printf("io_destroy error: %s\n", strerror(-ret));
        }
        free(buff);
        close(fd);
        return -1;
    }


    if ((ret = io_destroy(ctx) < 0))
    {
        printf("io_destroy error: %s\n", strerror(-ret));
        
        ret = -1;
    }
    else
    {
        ret = 0;
    }
    

    free(buff);
    close(fd);
    return ret;
}