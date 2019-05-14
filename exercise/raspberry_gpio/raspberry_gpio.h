#ifndef RASPBERRY_GPIO_H_
#define RASPBERRY_GPIO_H_

#include <stdint.h>
#include <linux/io.h>

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

#define GPIO_PIN_(x)        (uint32_t)(0x01 << ((x) % 32))


enum GPIO_IO_TYPE
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
};

enum GPIO_PULL_TYPE
{
    GPIO_PULL_TYPE_NO       = 0x00,
    GPIO_PULL_TYPE_DOWN     = 0x01,
    GPIO_PULL_TYPE_UP       = 0x02,
};

extern void __iomem *gpio_base;

inline void gpio_set_io_type(uint8_t index, enum GPIO_IO_TYPE iotype)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_FSEL_BASE - GPIO_BASE) + 4 * (index / 10)) / 4;
    uint32_t temp = iotype << (3 * (index % 3));
    iowrite32(temp, addr);
}

inline void gpio_set_bits_hight(uint32_t pins)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_SET_BASE - GPIO_BASE) + 4) / 4;
    iowrite32(pins & 0x3ffff, addr);
}

inline void gpio_set_bits_low(uint32_t pins)
{
    void __iomem *addr = gpio_base + (uint32_t)(GPIO_SET_BASE - GPIO_BASE) / 4;
    iowrite32(pins, addr);
}

inline void gpio_set_bit(uint8_t index)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_SET_BASE - GPIO_BASE) + index > 31 ? 4 : 0) / 4;
    iowrite32(GPIO_PIN_(index), addr);
}

inline void gpio_set_bits_hight(uint32_t pins)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_SET_BASE - GPIO_BASE) + 4) / 4;
    iowrite32(pins & 0x3ffff, addr);
}

inline void gpio_set_bits_low(uint32_t pins)
{
    void __iomem *addr = gpio_base + (uint32_t)(GPIO_SET_BASE - GPIO_BASE) / 4;
    iowrite32(pins, addr);
}

inline void gpio_set_bit(uint8_t index)
{
    void __iomem *addr = gpio_base + ((uint32_t)(GPIO_SET_BASE - GPIO_BASE) + index > 31 ? 4 : 0) / 4;
    iowrite32(GPIO_PIN_(index), addr);
}


inline void gpio_set_bits(uint32_t pins, uint32_t offset)
{
    void __iomem *addr
}


#endif