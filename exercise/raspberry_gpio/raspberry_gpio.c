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


static int rasp_gpio_major = 0;
module_param(rasp_gpio_major, int, S_IRUGO);

static int rasp_gpio_size = RASP_GPIO_SIZE;
module_param(rasp_gpio_size, int, S_IRUGO);

static uint rasp_gpio_phys_addr = 0x00;
module_param(rasp_gpio_phys_addr, uint, S_IRUGO);

struct rasp_gpio_pin
{
    bool detect_enable;
    bool pull_enable;
    uint8_t detect_type,;
    uint8_t status;
    uint8_t io_type;
};

static inline void rasp_gpio_pin_init(struct rasp_gpio_pin *gpio_pin)
{
    gpio_pin->detect_enable = false;
    gpio_pin->detect_type = RASP_GPIO_DETECT_TYPE_NONE;
    gpio_pin->pull_enable = false;
    gpio_pin->status = RASP_GPIO_STATUS_UNAVAILABLE;
    gpio_pin->io_type = GPIO_IO_TYPE_DEFAULT,
}

struct rasp_gpio_dev
{
    uint8_t pull_type;
    struct rasp_gpio_pin *pins;
    struct cdev cdev;
};

void __iomem *gpio_base = NULL;
struct rasp_gpio_dev *rasp_gpio_devp;


inline int gpio_set_io_type(uint8_t index, uint8_t iotype)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_FSEL_BASE - GPIO_BASE) + 4 * (index / 10)) / 4;
    if (iotype > 0x07)
    {
        return -EINVAL;
    }
    uint32_t temp = iotype << (3 * (index % 3));
    iowrite32(temp, addr);
    return 0;
}

inline uint8_t gpio_get_io_type(uint8_t index)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_FSEL_BASE - GPIO_BASE) + 4 * (index / 10)) / 4;
    volatile uint32_t temp = ioread32(addr);
    return temp >> (3 * (index % 3));
}

inline void _gpio_set_bits(uint32_t pins, uint32_t offset)
{
    void __iomem *addr = gpio_base + (uint32_t)(offset - GPIO_BASE) / 4;
    iowrite32(pins, offset);
}

inline uint32_t _gpio_get_bits(uint32_t offset)
{
    void __iomem *addr = gpio_base + (uint32_t)(offset - GPIO_BASE) / 4;
    volatile uint32_t ret = ioread32(addr);
    return ret;
}

inline void gpio_set_bits_hight(uint32_t pins)
{
    _gpio_set_bits(pins & 0x3fffff, GPIO_SET_BASE + 4);
}

inline void _gpio_set_bits_low(uint32_t pins)
{
    _gpio_set_bits(pins, GPIO_SET_BASE);
}

#define gpio_set_bit(index) \
do  \
{   \
    if (index > 32) \
    {   \
        gpio_set_bits_hight(GPIO_PIN_(index));  \
    }   \
    else    \
    {   \
        gpio_set_bits_low(GPIO_PIN_(index));    \
    }   \
} while(0)


inline void gpio_clear_bits_hight(uint32_t pins)
{
    _gpio_set_bits(pins & 0x3fffff, GPIO_CLEAR_BASE + 4);
}

inline void gpio_clear_bits_low(uint32_t pins)
{
    _gpio_set_bits(pins, GPIO_CLEAR_BASE);
}

#define gpio_clear_bit(index) \
do  \
{   \
    if ((index) > 32)   \
    {   \
        gpio_set_bits_hight(GPIO_PIN_(index));  \
    }   \
    else    \
    {   \
        gpio_set_bits_low(GPIO_PIN_(index));    \
    }   \
} while (0)

inline uint32_t gpio_levels_hight(void)
{
    return _gpio_get_bits(GPIO_LEV_BASE + 4);
}

inline uint32_t gpio_levels_low(void)
{
    return _gpio_get_bits(GPIO_LEV_BASE);
}

#define gpio_level_is_hight(index)  \
index > 32 ? \
    (gpio_levels_hight() & GPIO_PIN_(index) > 0 ? true : false) :   \
    (gpio_levels_low() & GPIO_PIN_(index) > 0 ? true : false)   \

#define _gpio_detect_enable(index, y, offset) \
do  \
{   \
    void __iomem *addr = gpio_base + (offset) - GPIO_BASE + ((index) > 32 ? 4 : 0) / 4; \
    volatile uint32_t temp = ioread32(addr);    \
    if (y)  \
    {   \
       temp |= GPIO_PIN_(index);   \
    }   \
    else    \
    {   \
       temp &= (!GPIO_PIN_(index));    \
    }   \
    iowrite32(temp, addr);  \
} while (0)


inline void gpio_detect_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_EDS_BASE);
}

#define _gpio_detect_status(index, offset)  \
((_gpio_get_bits((offset) + ((index) > 32 ? 4 : 0)) & GPIO_PIN_(index)) > 0 ? true : false)

inline bool gpio_detect_status(uint8_t index)
{
    return _gpio_detect_status(index);
}

inline void gpio_detect_rising_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_REN_BASE);
}

inline bool gpio_detect_rising_status(uint8_t index)
{
    return _gpio_detect_status(index, GPIO_REN_BASE);
}

inline void gpio_detect_falling_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_REN_BASE);
}

inline bool gpio_detect_falling_status(uint8_t index)
{
    return _gpio_detect_status(index, GPIO_REN_BASE);
}

inline void gpio_detect_hight_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_HEN_BASE);
}

inline bool gpio_detect_hight_status(uint8_t index)
{
    return _gpio_detect_status(index, GPIO_HEN_BASE);
}

inline void gpio_detect_low_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_LDN_BASE);
}

inline bool gpio_detect_low_statu(uint8_t index)
{
    return _gpio_detect_status(index, GPIO_LDN_BASE);
}

inline void gpio_detect_async_rising_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_AREN_BASE);
}

inline bool gpio_detect_async_rising_status(uint8_t index, bool y)
{
    return _gpio_detect_status(index, y, GPIO_AREN_BASE);
}

inline void gpio_dected_async_falling_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_AFEN_BASE);
}

inline bool gpio_dected_async_falling_enable(uint8_t index)
{
    return _gpio_detect_status(index, GPIO_AFEN_BASE);
}

inline void gpio_set_pull_type(enum GPIO_PULL_TYPE pull_type)
{
    _gpio_set_bits(pull_type & 0x07, GPIO_PULL_BASE)
}

inline enum GPIO_PULL_TYPE gpio_get_pull_type(void)
{
    (enum GPIO_PULL_TYPE)(_gpio_get_bits(GPIO_PULL_BASE));
}

#define _gpio_pull_enable(index, y, offset) _gpio_detect_enable(index, y, offset)

#define _gpio_pull_status(index, y, offset) _gpio_detect_status(index, y, offset)

inline void gpio_pull_enable(uint8_t index, bool y)
{
    _gpio_pull_enable(index, y, GPIO_PULL_CLK_BASE);
}

inline bool gpio_pull_status(uint8_t index)
{
    return _gpio_pull_status(index, GPIO_PULL_CLK_BASE);
}

static int rasp_gpio_open(struct inode *inode, struct file *filp)
{
    nonseekable_open(inode, filp);
    return 0;
}

static void rasp_gpio_release(struct inode *inode, struct file *filp)
{
    
}

static ssize_t rasp_gpio_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    return 0;
}

static ssize_t rasp_gpio_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    return 0;
}

static long rasp_gpio_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
    long ret;
    uint8_t index, value;

    if (_IOC_TYPE(cmd) != RASP_GPIO_IOC_MAGIC)
    {
        return -EINVAL;
    }

    if (_IOC_NR(cmd) > RASP_GPIO_IOC_MAX)
    {
        return -EINVAL;
    }
    index = args & 0xff;
    value = (args >> 8) & 0xff;

    if (index > rasp_gpio_size)
    {
        return -EINVAL;
    }

    if (rasp_gpio_devp->pins[index]->status != RASP_GPIO_STATUS_USEING)
    {
        return -EFAULT;
    }

    switch (cmd)
    {
    case RASP_GPIO_IOC_SET_IO_TYPE:
        ret = gpio_set_io_type(index, value);
        if (ret == 0)
        {
            rasp_gpio_devp->pins[index]
        }
        break;
    
    case RASP_GPIO_IOC_GET_IO_TYPE:
        ret = gpio_get_io_type(index);
        break;

    case RASP_GPIO_IOC_SET_B
    default:
        break;
    }
}