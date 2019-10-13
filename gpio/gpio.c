
// #include <stdint.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/timer.h>

#include <asm-generic/io.h>

#define GPIO_BASE           0x00200000  // base offset
#define GPIO_FSEL_BASE      0x00200000  // function setting
#define GPIO_SET_BASE       0x0020001C  // set bit
#define GPIO_CLEAR_BASE     0x00200028  // clear bit
#define GPIO_LEV_BASE       0x00200034  // return the actuak value of the pin
#define GPIO_EDS_BASE       0x00200040  // event detect status
#define GPIO_REN_BASE       0x0020004C  // rising edge detect enable
#define GPIO_FEN_BASE       0x00200058  // failing edge detect enable
#define GPIO_HEN_BASE       0x00200064  // hight detect enable
#define GPIO_LDN_BASE       0x00200070  // low detect enable
#define GPIO_AREN_BASE      0x0020007C  // asynchronous rising edge detect enable (it use to detected very short rising edges)
#define GPIO_AFEN_BASE      0x00200088  // asynchronous failling edge detect enable (it use to detected very short faillinfg edges)
#define GPIO_UD_BASE        0x00200094  // pull-up/down 150 colck
#define GPIO_UD_CLK_BASE    0x00200098  // pull-up/dowm enable 150clock
#define GPIO_BASE_LAST      0x002000B0
#define GPIO_PIN_(x)        (uint32_t)(0x01 << ((x) % 32))

#undef PRINT_INFO
#define PRINT_INFO(fmt, args...) printk(KERN_INFO "gpio: " fmt, ## args)

static unsigned int phys_addr = 0;
module_param(phys_addr, uint, S_IRUGO);

typedef enum 
{
    GPIO_IO_TYPE_DEFAULT    = 0x00,
    GPIO_IO_TYPE_IN         = 0x00,
    GPIO_IO_TYPE_OUT        = 0x01,
    GPIO_IO_TYPE_FN0        = 0x04,
    GPIO_IO_TYPE_FN1        = 0x05,
    GPIO_IO_TYPE_FN2        = 0x06,
    GPIO_IO_TYPE_FN3        = 0x07,
    GPIO_IO_TYPE_FN4        = 0x03,
    GPIO_IO_TYPE_FN5        = 0x02,
} GPIO_IO_TYPE;

typedef enum 
{
    GPIO_PULL_TYPE_NO       = 0x00,
    GPIO_PULL_TYPE_DOWN     = 0x01,
    GPIO_PULL_TYPE_UP       = 0x02,
} GPIO_PULL_TYPE;

void __iomem *gpio_base;

static int __init gpio_init(void)
{   
    if (phys_addr == 0)
    {
        PRINT_INFO("input phys_add", phys_addr);
        return -EFAULT;
    }
    // phys_addr += GPIO_BASE / 4;
    
    PRINT_INFO("phys_add: 0x%x\n", phys_addr);

    gpio_base = ioremap(phys_addr, GPIO_BASE_LAST - GPIO_BASE);
    if (!gpio_base)
    {
        PRINT_INFO("ioremap fauit\n");
        return -EFAULT;
    }
    PRINT_INFO("virtual add: %p", gpio_base);

    return 0;
}

module_init(gpio_init);

static void __exit gpio_exit(void)
{
    if (gpio_base)
    {
        iounmap(gpio_base);
    }
}

module_exit(gpio_exit);

MODULE_LICENSE("GPL v2");
