#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/io.h>
#include <asm-generic/io.h>

#include <linux/types.h>
#include <linux/stdint.h>

phys_addr_t physics_addr;

volatile void __iomem *virtual_addr;
volatile void __iomem *GPL2CON, *GPL2DAT, *GPL2PUD;

#define PHYSICS_ADDRESS		0x11000100


static void gpl2_device_init(void)
{

	printk(KERN_EMERG "led gpl2: %s", __func__);
	physics_addr = (phys_addr_t)(PHYSICS_ADDRESS);

	virtual_addr = ioremap(physics_addr, 0x10);

	GPL2CON = virtual_addr + 0x00;
	GPL2DAT = virtual_addr + 0x04;
	GPL2PUD = virtual_addr + 0x08;

}


static void gpl2_configure(void)
{
	uint32_t temp;

	printk(KERN_EMERG "led gpl2: %s", __func__);
	temp = ioread32(GPL2CON);
	temp &= 0xfffffff1;
	temp |= 0x00000001;
	iowrite32(temp, GPL2CON);

	temp = ioread16(GPL2PUD);
	temp |= 0x0003;
	iowrite16(GPL2PUD, uint16(temp & 0xffff));
}


static void gpl2_on(void)
{
	printk(KERN_EMERG "led gpl2: %s", __func__);
	uint8_t temp;
	temp = ioread8(GPL2DAT);
	temp |= 0x01;
	iowrite8(GPL2DAT, temp);
}

static void gpl2_off(void)
{
	printk(KERN_EMERG "led gpl2: %s", __func__);
	uint8_t temp;
	temp = ioread8(GPL2DAT);
	temp &= 0xfe;
	iowrite8(GPL2DAT, temp);
}

static int __init led_gpl2_init(void)
{
	printk(KERN_EMERG "led gpl2 module enter");
	gpl2_device_init();
	gpl2_configure();
	gpl2_on();
	return 0;
}

module_init(led_gpl2_init);

static void __exit led_gpl2_exit(void)
{
	printk(KERN_EMERG "led gpl2 module exit");

	gpl2_off();
	iounmap(virtual_addr);
}

module_exit(led_gpl2_exit);

MODULE_LICENSE("GPL v2");
