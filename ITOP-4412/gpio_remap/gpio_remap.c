#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/io.h>
#include <linux/ioport.h>
#include <arch/arm/include/asm/io.h>



#include <linux/types.h>


phys_addr_t physics_addr;

volatile uint32_t virtual_addr;
volatile uint32_t GPL2CON, GPL2DAT, GPL2PUD;

#define PHYSICS_ADDRESS		0x11000100

static struct resource *gpio_resource;

static void gpl2_device_init(void)
{
	physics_addr = (phys_addr_t)(PHYSICS_ADDRESS);

	printk(KERN_EMERG "led gpl2: %s", __func__);
	gpio_resource = request_mem_region(physics_addr, 0x10, "GPL2");

	if (!gpio_resource)
	{
		printk(KERN_EMERG "request_mem_region failed");
	}
	virtual_addr = (uint32_t)(ioremap(physics_addr, 0x10));

	printk(KERN_EMERG "led gpl2: virtual address: 0x%x", virtual_addr);
	GPL2CON = virtual_addr + 0x00;
	GPL2DAT = virtual_addr + 0x04;
	GPL2PUD = virtual_addr + 0x08;

	printk(KERN_EMERG "led gpl2: GPL2CON address: 0x%x", GPL2CON);
	printk(KERN_EMERG "led gpl2: GPL2DAT address: 0x%x", GPL2DAT);
	printk(KERN_EMERG "led gpl2: GPL2PUD address: 0x%x", GPL2PUD);
}


static void gpl2_configure(void)
{
	uint32_t temp;
	int i = 0;
	printk(KERN_EMERG "led gpl2: %s", __func__);

	// temp = *GPL2CON;
	printk(KERN_EMERG "led gpl2: %i", i++);
	temp = ioread32(0xefb7a100);
	
	temp &= 0xfffffff1;
	temp |= 0x00000001;
	printk(KERN_EMERG "led gpl2: %i", i++);
	iowrite32(temp, 0xefb7a100);

	// *GPL2CON = temp;

	// temp = *GPL2PUD;
	printk(KERN_EMERG "led gpl2: %i", i++);
	temp = ioread32(0xefb7a108);
	temp |= 0x0003;
	printk(KERN_EMERG "led gpl2: %i", i++);
	iowrite32(temp, 0xefb7a108);
	printk(KERN_EMERG "led gpl2: %i", i++);
	// *GPL2PUD = temp;
}


static void gpl2_on(void)
{
	uint8_t temp;
	printk(KERN_EMERG "led gpl2: %s", __func__);
	int i = 0;
	printk(KERN_EMERG "led gpl2: %i", i++);
	temp = ioread8(GPL2DAT);
	// temp = *GPL2DAT;
	temp |= 0x01;
	printk(KERN_EMERG "led gpl2: %i", i++);
	iowrite8(temp, GPL2DAT);
	printk(KERN_EMERG "led gpl2: %i", i++);
	//*GPL2DAT = temp;
}

static void gpl2_off(void)
{
	uint8_t temp;
	printk(KERN_EMERG "led gpl2: %s", __func__);
	
	temp = ioread8(GPL2DAT);
	// temp = *GPL2DAT;
	temp &= 0xfe;
	iowrite8(temp, GPL2DAT);
	// *GPL2DAT = temp;
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
