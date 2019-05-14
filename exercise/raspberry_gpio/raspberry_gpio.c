#include "raspberry_gpio.h"

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




void __iomem *gpio_base;






#include <stdio.h>

int main()
{   
    int i = 0;
    for (; i < 6; ++i)
    {
        printf("GPFSEL%i: 0x%x\n", i, GPIO_FSEL(i));
    }
    return 0;
}