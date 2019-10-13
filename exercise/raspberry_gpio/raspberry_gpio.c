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

#include <linux/stat.h>
#include <linux/uaccess.h>

#include <linux/io.h>
#include <linux/ioport.h>
#include <arch/arm/include/asm/io.h>

#include <linux/delay.h>
#include <linux/interrupt.h>

static int rasp_gpio_major = 0;
module_param(rasp_gpio_major, int, S_IRUGO);

static int rasp_gpio_size = RASP_GPIO_SIZE;
module_param(rasp_gpio_size, int, S_IRUGO);

static uint rasp_gpio_phys_addr = 0x00;
module_param(rasp_gpio_phys_addr, uint, S_IRUGO);



static inline void rasp_gpio_pin_init(struct rasp_gpio_pin *gpio_pin)
{
    gpio_pin->detect_enable = false;
    gpio_pin->detect_type = GPIO_DETECT_TYPE_NONE;
    gpio_pin->pull_enable = false;
    gpio_pin->status = GPIO_STATUS_UNAVAILABLE;
    gpio_pin->io_type = GPIO_IO_TYPE_DEFAULT;
}




void __iomem *gpio_base = NULL;
struct rasp_gpio_dev *rasp_gpio_devp;


static inline bool gpio_set_status(uint8_t index, uint8_t status)
{
    if ((rasp_gpio_devp->pins+index)->status == GPIO_STATUS_UNAVAILABLE)
    {
        return false;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->status = status;
        return true;
    }
}

static inline uint8_t gpio_get_status(uint8_t index)
{
    return (rasp_gpio_devp->pins+index)->status;
}



inline bool gpio_set_io_type(uint8_t index, uint8_t io_type)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_FSEL_BASE - GPIO_BASE) + 4 * (index / 10)) ;
    volatile uint32_t temp = ioread32(addr);
    PRINT_INFO("current fsel%d: 0x%x\n", index / 10, temp);
    if (io_type > 0x07)
    {
        return -EINVAL;
    }
    temp |= (io_type << (index % 10 * 3));
    PRINT_INFO("temp: 0x%x\n", temp);
    iowrite32(temp, addr);
    barrier();
    (rasp_gpio_devp->pins+index)->io_type = io_type;
    return true;
}

inline uint8_t gpio_get_io_type(uint8_t index)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_FSEL_BASE - GPIO_BASE) + 4 * (index / 10));
    volatile uint32_t temp = ioread32(addr);
    (rasp_gpio_devp->pins+index)->io_type = (uint8_t)(temp >> (3 * (index % 3)) & 0xff);
    return (rasp_gpio_devp->pins+index)->io_type;
}

inline void _gpio_set_bits(uint32_t pins, uint32_t offset)
{
    void __iomem *addr = gpio_base + (uint32_t)(offset - GPIO_BASE);
    PRINT_INFO("offset: 0x%x\n", offset - GPIO_BASE);
    iowrite32(pins, addr);
    barrier();
}

inline uint32_t _gpio_get_bits(uint32_t offset)
{
    void __iomem *addr = gpio_base + (uint32_t)(offset - GPIO_BASE);
    volatile uint32_t ret = ioread32(addr);
    return ret;
}

inline void gpio_set_bits_hight(uint32_t pins)
{
    _gpio_set_bits(pins, GPIO_SET_BASE + 4);
}

inline void gpio_set_bits_low(uint32_t pins)
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
    barrier();  \
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
        gpio_clear_bits_hight(GPIO_PIN_(index));  \
    }   \
    else    \
    {   \
        gpio_clear_bits_low(GPIO_PIN_(index));    \
    }   \
    barrier();  \
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
    ((gpio_levels_hight() & GPIO_PIN_(index)) > 0 ? true : false) :   \
    ((gpio_levels_low() & GPIO_PIN_(index)) > 0 ? true : false)   \

#define _gpio_detect_enable(index, y, offset) \
do  \
{   \
    void __iomem *addr = gpio_base + (offset) - GPIO_BASE + ((index) > 32 ? 4 : 0); \
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
    barrier();  \
} while (0)


inline void gpio_detect_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_EDS_BASE);
    ((rasp_gpio_devp->pins+index))->detect_enable = true;
}

#define _gpio_detect_status(index, offset)  \
((_gpio_get_bits((offset) + ((index) > 32 ? 4 : 0)) & GPIO_PIN_(index)) > 0 ? true : false)

inline bool gpio_detect_status(uint8_t index)
{
    (rasp_gpio_devp->pins+index)->detect_enable = _gpio_detect_status(index, GPIO_EDS_BASE);
    return (rasp_gpio_devp->pins+index)->detect_enable;
}

inline void gpio_detect_rising_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_REN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_RASING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_RASING);
    }
}

inline bool gpio_detect_rising_status(uint8_t index)
{
    bool ret = _gpio_detect_status(index, GPIO_REN_BASE);
    if (ret)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_RASING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_RASING);
    }
    return ret;
}

inline void gpio_detect_falling_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_REN_BASE);
     if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_FALLING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_FALLING);
    }
}

inline bool gpio_detect_falling_status(uint8_t index)
{
    bool y = _gpio_detect_status(index, GPIO_REN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_FALLING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_FALLING);
    }
    return y;
}

inline void gpio_detect_hight_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_HEN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_HIGHT;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_HIGHT);
    }
}

inline bool gpio_detect_hight_status(uint8_t index)
{
    bool y = _gpio_detect_status(index, GPIO_HEN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_HIGHT;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_HIGHT);
    }
    return y;
}

inline void gpio_detect_low_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_LDN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_LOW;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_LOW);
    }
}

inline bool gpio_detect_low_status(uint8_t index)
{
    bool y =  _gpio_detect_status(index, GPIO_LDN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_LOW;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_LOW);
    }
    return y;
}

inline void gpio_detect_async_rising_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_AREN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_ASYNC_RASING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_ASYNC_RASING);
    }
}

inline bool gpio_detect_async_rising_status(uint8_t index)
{
    bool y = _gpio_detect_status(index, GPIO_AREN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_ASYNC_RASING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_ASYNC_RASING);
    }
    return y;
}

inline void gpio_detect_async_falling_enable(uint8_t index, bool y)
{
    _gpio_detect_enable(index, y, GPIO_AFEN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_ASYNC_FALLING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_ASYNC_FALLING);
    }
}

inline bool gpio_detect_async_falling_status(uint8_t index)
{
    bool y = _gpio_detect_status(index, GPIO_AFEN_BASE);
    if (y)
    {
        (rasp_gpio_devp->pins+index)->detect_type |= GPIO_DETECT_TYPE_ASYNC_FALLING;
    }
    else
    {
        (rasp_gpio_devp->pins+index)->detect_type &= (!GPIO_DETECT_TYPE_ASYNC_FALLING);
    }
    return y;
}

inline void gpio_set_pull_type(uint8_t pull_type)
{
    _gpio_set_bits(pull_type & 0x07, GPIO_PULL_BASE);
    rasp_gpio_devp->pull_type = pull_type;
}

inline uint8_t gpio_get_pull_type(void)
{
    rasp_gpio_devp->pull_type = _gpio_get_bits(GPIO_PULL_BASE);
    return rasp_gpio_devp->pull_type;
}

#define _gpio_pull_enable(index, y, offset) _gpio_detect_enable(index, y, offset)

#define _gpio_pull_status(index, offset) _gpio_detect_status(index, offset)

inline void gpio_pull_enable(uint8_t index, bool y)
{
    _gpio_pull_enable(index, y, GPIO_PULL_CLK_BASE);
    (rasp_gpio_devp->pins+index)->pull_enable = y;
}

inline bool gpio_pull_status(uint8_t index)
{
    return (rasp_gpio_devp->pins+index)->pull_enable = _gpio_pull_status(index, GPIO_PULL_CLK_BASE);
}

static int rasp_gpio_open(struct inode *inode, struct file *filp)
{
    nonseekable_open(inode, filp);
    return 0;
}

static int rasp_gpio_release(struct inode *inode, struct file *filp)
{
    return 0;
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

   if (gpio_get_status(index) == GPIO_STATUS_UNAVAILABLE)
   {
       return -EFAULT;
   }

    switch (cmd)
    {
    case RASP_GPIO_IOC_SET_IO_TYPE:
        ret = gpio_set_io_type(index, value);
        break;
    
    case RASP_GPIO_IOC_GET_IO_TYPE:
        ret = gpio_get_io_type(index);
        break;

    case RASP_GPIO_IOC_SET_DETECT:
        gpio_detect_enable(index, value > 0 ? true : false);
        ret = true;

    case RASP_GPIO_IOC_GET_DETECT:
        ret = gpio_detect_status(index);
        break;

    case RASP_GPIO_IOC_SET_RISING:
        gpio_detect_rising_enable(index, value > 0 ? true : false);
        ret = true;
        break;

    case RASP_GPIO_IOC_GET_RISING:
        ret = gpio_detect_rising_status(index);
        break;

    case RASP_GPIO_IOC_SET_FALLING:
        gpio_detect_falling_enable(index, value > 0 ? true : false);
        ret = true;
        break;

    case RASP_GPIO_IOC_GET_FALLING:
        ret = gpio_detect_falling_status(index);
        break;



    case RASP_GPIO_IOC_GET_HIGHT:
        ret = gpio_detect_hight_status(index);
        break;

    case RASP_GPIO_IOC_SET_HIGHT:
        gpio_detect_hight_enable(index, value > 0 ? true : false);
        ret = true;
        break;

    case RASP_GPIO_IOC_GET_LOW:
        ret = gpio_detect_low_status(index);
        break;

    case RASP_GPIO_IOC_SET_LOW:
        gpio_detect_low_enable(index, value > 0 ? true : false);
        ret = true;
        break;

    case RASP_GPIO_IOC_GET_A_RISING:
        ret = gpio_detect_async_rising_status(index);
        break;

    case RASP_GPIO_IOC_SET_A_RISING:
        gpio_detect_async_rising_enable(index, value > 0 ? true : false);
        ret = true;
        break;

    case RASP_GPIO_IOC_GET_A_FALLING:
        ret = gpio_detect_async_falling_status(index);
        break;

    case RASP_GPIO_IOC_SET_A_FALLING:
        gpio_detect_async_falling_enable(index, value > 0 ? true : false);
        ret = true;
        break;

    case RASP_GPIO_IOC_SET_PULL_TYPE:
        if (value > GPIO_PULL_TYPE_UP)
        {
            ret = -EINVAL;
        }
        else
        {
            gpio_set_pull_type(value);
        }
        break;

    case RASP_GPIO_IOC_GET_PULL_TYPE:
        ret = gpio_get_pull_type() ;
        break;

    case RASP_GPIO_IOC_GET_PULL:
        ret = gpio_pull_status(index);
        break;

    case RASP_GPIO_IOC_SET_PULL:
        gpio_pull_enable(index, value > 0 ? true : false);
        ret = true;
        break;

    case RASP_GPIO_IOC_SET_BIT:
        gpio_set_bit(index);
        ret = true;
        break;

    case RASP_GPIO_IOC_CLEAR_BIT:
        gpio_clear_bit(index);
        ret = true;
        break;

    case RASP_GPIO_IOC_GET_LEVEL:
        ret = gpio_level_is_hight(index);
        break;

    case RASP_GPIO_IOC_SET_STATUS:
        ret = gpio_set_status(index, value);
        break;

    case RASP_GPIO_IOC_GET_STATUS:
        ret = gpio_get_status(index);
        break; 

    default:
        ret = -EINVAL;
    }

    return ret;
}

static unsigned int rasp_gpio_poll(struct file *filp, poll_table *wait)
{
    return POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;
}

static const struct file_operations rasp_gpio_fops = 
{
    .owner = THIS_MODULE,
    .open = rasp_gpio_open,
    .release = rasp_gpio_release,
    .read = rasp_gpio_read,
    .write = rasp_gpio_write,
    .unlocked_ioctl = rasp_gpio_ioctl,
    .poll = rasp_gpio_poll,
};


static int rasp_gpio_seq_show(struct seq_file *sfilp, void *data)
{
    uint i = GPIO_BASE;
    seq_printf(sfilp, "raspberry register values:\n");
    PRINT_INFO("phy address: 0x%x\n", rasp_gpio_phys_addr);
    PRINT_INFO("virtual address: 0x%x\n", gpio_base);
    volatile uint32_t temp;
    void __iomem *addr;
    
    for (; i < GPIO_BASE_LAST; i += 4)
    {   
        addr = gpio_base + (i - GPIO_BASE);
        temp = ioread32(addr);
        seq_printf(sfilp, "\toffset: 0x%x\tvalue: %p\n", i, temp);
    }

    return 0;
}

static int rasp_gpio_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, &rasp_gpio_seq_show, NULL);
}

static const struct file_operations rasp_gpio_proc_fops = 
{
    .owner = THIS_MODULE,
    .open = rasp_gpio_proc_open,
    .release = seq_release,
    .read = seq_read,
};

static void rasp_gpio_proc_create(void)
{
    proc_create("rasp_gpio", 0, NULL, &rasp_gpio_proc_fops);
}

static void rasp_gpio_proc_remove(void)
{
    remove_proc_entry("rasp_gpio", NULL);
}


static void timer_fn(struct timer_list *timer)
{
    static unsigned int i = 0;

    PRINT_INFO("gpio pin 2, 4 will change status");

    if (i % 2 == 0)
    {
        gpio_set_bit(2);
        gpio_clear_bit(4);
    }
    else
    {
        gpio_clear_bit(2);
        gpio_set_bit(4);
    }

    i ++;

    timer->expires = jiffies + HZ;

    add_timer(timer);    
}

static struct timer_list global_timer;

static int kernel_probe(int count)
{
    int irq = 0;

    gpio_set_bit(2);
    mdelay(1);
    do 
    {
        unsigned long mask = probe_irq_on();
        gpio_clear_bit(2);
        mdelay(1);
        irq = probe_irq_off(mask);

        if (irq == 0)
        {
            PRINT_INFO("no irq reported by probe\n");
            irq = -1;
        }
        gpio_set_bit(2);
        mdelay(1);
        count --;
    } while (count > 0 && irq < 0);

    return irq;
}

int rasp_riq;

irqreturn_t rasp_gpio_probing(int irq, void *dev_id, struct pt_regs *regs)
{
    PRINT_INFO("into to the interrupt\n");
    return IRQ_HANDLED;
}
static int __init rasp_gpio_init(void)
{
    int ret;
    dev_t devno = MKDEV(rasp_gpio_major, 0);
    int i;
    volatile uint32_t temp;
    void __iomem *addr;

    if (rasp_gpio_phys_addr == 0)
    {
        printk(KERN_EMERG "please insmod module with raspberry's gpio phys address\n");
        return -EFAULT;
    }

    if (rasp_gpio_major == 0)
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "raspberry gpio");
        rasp_gpio_major = MAJOR(devno);
    }
    else
    {
        ret = register_chrdev_region(devno, 1, "raspberry gpio");
    }

    if (ret < 0)
    {
        PRINT_INFO("register region fault, error code: %ld", ret);
        return ret;
    }
    
    gpio_base = ioremap(rasp_gpio_phys_addr, GPIO_BASE_LAST - GPIO_BASE);
    PRINT_INFO("phy address: 0x%x\n", rasp_gpio_phys_addr);
    PRINT_INFO("virtual address: 0x%x\n", gpio_base);
    
    if (gpio_base == NULL)
    {
        PRINT_INFO("can't remap io address, phys address: %p, size: %ld", rasp_gpio_phys_addr, GPIO_BASE_LAST - GPIO_BASE);
        unregister_chrdev_region(devno, 1);
        return -EFAULT;
    }

    rasp_gpio_devp = kzalloc(sizeof(struct rasp_gpio_dev), GFP_KERNEL);

    if (rasp_gpio_devp == NULL)
    {
        unregister_chrdev_region(devno, 1);
        iounmap(gpio_base);
        return -ENOMEM;
    }

    rasp_gpio_devp->pins = kzalloc(rasp_gpio_size * sizeof(struct rasp_gpio_pin), GFP_KERNEL);

    if (rasp_gpio_devp->pins == NULL)
    {
        kfree(rasp_gpio_devp);
        unregister_chrdev_region(devno, 1);
        iounmap(gpio_base);
        return -ENOMEM;   
    }

    rasp_gpio_devp->pull_type = GPIO_PULL_TYPE_NO;
    for (i = 0; i < rasp_gpio_size; ++i)
    {
        rasp_gpio_pin_init(rasp_gpio_devp->pins + i);
    }
    
    /*
        
    */
    cdev_init(&rasp_gpio_devp->cdev, &rasp_gpio_fops);
    rasp_gpio_devp->cdev.owner = THIS_MODULE;

    ret = cdev_add(&rasp_gpio_devp->cdev, devno, 1);
    

    if (ret < 0)
    {
        kfree(rasp_gpio_devp->pins);
        kfree(rasp_gpio_devp);
        unregister_chrdev_region(devno, 1);
        iounmap(gpio_base);
        return -EFAULT;
    }
    rasp_gpio_proc_create();

    

    gpio_set_status(2, GPIO_STATUS_USEING);
    gpio_set_status(3, GPIO_STATUS_USEING);
    gpio_set_status(4, GPIO_STATUS_USEING);


    gpio_set_io_type(2, GPIO_IO_TYPE_OUT);
    gpio_set_io_type(3, GPIO_IO_TYPE_IN);
    gpio_set_io_type(4, GPIO_IO_TYPE_OUT);

    gpio_set_bit(2);
    gpio_set_bit(4);

    gpio_detect_enable(3);
    gpio_detect_falling_enable(3);

    rasp_irq = kernel_probe(10);

    if (rasp_riq < 0)
    {
        PRINT_INFO("can't find the irq port\n");
    }
    else
    {
        ret = request_irq(rasp_irq, rasp_gpio_probing, SA_INTERRUPT, "rasp gpio", NULL);
        if (ret == 0)
        {
            PRINT_INFO("request irq successed\n");
        }
        ret = 0;
    }

    timer_setup(&global_timer, &timer_fn, TIMER_SOFTIRQ);
    global_timer.expires = jiffies + HZ;
    add_timer(&global_timer);

    return ret;
}

module_init(rasp_gpio_init);

static void __exit rasp_gpio_exit(void)
{

    del_timer_sync(&global_timer);
    if (rasp_irq > 0)
    {
        free_irq(rasp_irq);
    }

    INIT_WORK()
    PREPARE_WORK()
    rasp_gpio_proc_remove();

    unregister_chrdev_region(MKDEV(rasp_gpio_major, 0), 1);
    cdev_del(&rasp_gpio_devp->cdev);

    kfree(rasp_gpio_devp->pins);
    kfree(rasp_gpio_devp);
    
    iounmap(gpio_base);

}

module_exit(rasp_gpio_exit);

MODULE_LICENSE("GPL v2");