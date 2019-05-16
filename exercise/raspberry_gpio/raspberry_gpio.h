#ifndef RASPBERRY_GPIO_H_
#define RASPBERRY_GPIO_H_

#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/types.h>

#define GPIO_BASE 0x00200000          // base offset
#define GPIO_FSEL_BASE 0x00200000     // function setting
#define GPIO_SET_BASE 0x0020001C      // set bit
#define GPIO_CLEAR_BASE 0x00200028    // clear bit
#define GPIO_LEV_BASE 0x00200034      // return the actuak value of the pin
#define GPIO_EDS_BASE 0x00200040      // event detect status
#define GPIO_REN_BASE 0x0020004C      // rising edge detect enable
#define GPIO_FEN_BASE 0x00200058      // failing edge detect enable
#define GPIO_HEN_BASE 0x00200064      // hight detect enable
#define GPIO_LDN_BASE 0x00200070      // low detect enable
#define GPIO_AREN_BASE 0x0020007C     // asynchronous rising edge detect enable (it use to detected very short rising edges)
#define GPIO_AFEN_BASE 0x00200088     // asynchronous failling edge detect enable (it use to detected very short faillinfg edges)
#define GPIO_PULL_BASE 0x00200094     // pull-up/down 150 colck
#define GPIO_PULL_CLK_BASE 0x00200098 // pull-up/dowm enable 150clock
#define GPIO_BASE_LAST 0x002000B0

#define GPIO_PIN_(x) (uint32_t)(0x01 << ((x) % 32))


#undef PRINT_INFO
#ifdef __KERNEL___
#   define PRINT_INFO(fmt, args...) printk(KERN_INFO "raspberry gpio: " fmt, ##args)
#else
#   define PRINT_INFO(fmt, args...) printf(std)
#endif

#define RASP_GPIO_SIZE      53
typedef enum
{
    GPIO_STATUS_UNUSED = 0x00,
    GPIO_STATUS_USEING,
    GPIO_STATUS_UNAVAILABLE,
} GPIO_STATUS;
typedef enum
{
    GPIO_IO_TYPE_DEFAULT = 0x00,
    GPIO_IO_TYPE_IN = 0x00,
    GPIO_IO_TYPE_OUT = 0x01,
    GPIO_IO_TYPE_FN0 = 0x04,
    GPIO_IO_TYPE_FN1 = 0x05,
    GPIO_IO_TYPE_FN2 = 0x06,
    GPIO_IO_TYPE_FN3 = 0x07,
    GPIO_IO_TYPE_FN4 = 0x03,
    GPIO_IO_TYPE_FN5 = 0x02,
} GPIO_IO_TYPE;

typedef enum
{
    GPIO_PULL_TYPE_NO = 0x00,
    GPIO_PULL_TYPE_DOWN = 0x01,
    GPIO_PULL_TYPE_UP = 0x02,
} GPIO_PULL_TYPE;

typedef enum
{
    GPIO_DETECT_TYPE_NONE = 0x00,
    GPIO_DETECT_TYPE_RASING = 0x01 << 1,
    GPIO_DETECT_TYPE_FALLING = 0x01 << 2,
    GPIO_DETECT_TYPE_HIGHT = 0x01 << 3,
    GPIO_DETECT_TYPE_LOW = 0x01 << 4,
    GPIO_DETECT_TYPE_ASYNC_RASING = 0x01 << 5,
    GPIO_DETECT_TYPE_ASYNC_FALLING = 0x01 << 6,
} GPIO_DETECT_TYPE;

struct rasp_gpio_pin
{
    bool detect_enable;
    bool pull_enable;
    uint8_t detect_type,;
    uint8_t status;
    uint8_t io_type;
};

struct rasp_gpio_dev
{
    uint8_t pull_type;
    struct rasp_gpio_pin *pins;
    struct cdev cdev;
};

extern void __iomem *gpio_base;

#define RASP_GPIO_IOC_ARGS(index, value) (unsigned long)(value << 8 | index)

#define RASP_GPIO_IOC_MAGIC 'G'

#define RASP_GPIO_IOC_SET_IO_TYPE _IO(RASP_GPIO_IOC_MAGIC, 1)
#define RASP_GPIO_IOC_GET_IO_TYPE _IO(RASP_GPIO_IOC_MAGIC, 2)

#define RASP_GPIO_IOC_SET_DETECT _IO(RASP_GPIO_IOC_MAGIC, 3)
#define RASP_GPIO_IOC_GET_DETECT _IO(RASP_GPIO_IOC_MAGIC, 4)

#define RASP_GPIO_IOC_SET_RISING _IO(RASP_GPIO_IOC_MAGIC, 5)
#define RASP_GPIO_IOC_GET_RISING _IO(RASP_GPIO_IOC_MAGIC, 6)

#define RASP_GPIO_IOC_SET_FALLING _IO(RASP_GPIO_IOC_MAGIC, 7)
#define RASP_GPIO_IOC_GET_FALLING _IO(RASP_GPIO_IOC_MAGIC, 8)

#define RASP_GPIO_IOC_SET_HIGHT _IOW(RASP_GPIO_IOC_MAGIC, 9)
#define RASP_GPIO_IOC_GET_HIGHT _IO(RASP_GPIO_IOC_MAGIC, 10)

#define RASP_GPIO_IOC_SET_LOW _IOW(RASP_GPIO_IOC_MAGIC, 11)
#define RASP_GPIO_IOC_GET_LOW _IO(RASP_GPIO_IOC_MAGIC, 12)

#define RASP_GPIO_IOC_SET_A_RISING _IOW(RASP_GPIO_IOC_MAGIC, 13)
#define RASP_GPIO_IOC_GET_A_RISING _IO(RASP_GPIO_IOC_MAGIC, 14)

#define RASP_GPIO_IOC_SET_A_FALLING _IOW(RASP_GPIO_IOC_MAGIC, 15)
#define RASP_GPIO_IOC_GET_A_FALLING _IO(RASP_GPIO_IOC_MAGIC, 16)

#define RASP_GPIO_IOC_SET_PULL_TYPE _IO(RASP_GPIO_IOC_MAGIC, 17)
#define RASP_GPIO_IOC_GET_PULL_TYPE _IO(RASP_GPIO_IOC_MAGIC, 18)

#define RASP_GPIO_IOC_SET_PULL _IOW(RASP_GPIO_IOC_MAGIC, 19)
#define RASP_GPIO_IOC_GET_PULL _IO(RASP_GPIO_IOC_MAGIC, 20)

#define RASP_GPIO_IOC_SET_BIT _IO(RASP_GPIO_IOC_MAGIC, 21)
#define RASP_GPIO_IOC_CLEAR_BIT _IO(RASP_GPIO_IOC_MAGIC, 22)
#define RASP_GPIO_IOC_GET_LEVEL _IO(RASP_GPIO_IOC_MAGIC), 23)

#define RASP_GPIO_IOC_SET_STATUS  _IO(RASP_GPIO_IOC_MAGIC, 24)
#define RASP_GPIO_IOC_GET_STATUS _IO(RASP_GPIO_IOC_MAGIC, 25)

#define RASP_GPIO_IOC_MAX   26

#endif