#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/fcntl.h>

#include <linux/slab.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/platform_device.h>
#include <linux/device.h>

#include <plat/gpio-cfg.h>
#include <mach/gpio-exynos4.h>
#include <asm/gpio.h>


#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/io.h>

#include <linux/types.h>

#include <linux/irq.h>
#include <linux/interrupt.h>


#undef DEVICE_NAME
#define DEVICE_NAME "key"

int major = 0;
module_param(major, int, S_IRUGO);

struct key_dev
{
	int key[2];
	struct cdev cdev;
};

#undef PRINT
#define PRINT(fmt, args...) printk(KERN_INFO "%s: " fmt, DEVICE_NAME, ##args)

static int key_request_resource(struct key_dev *devp)
{
	char label[] = "key-0";
	int i, ret = 0;
	for (i = 0; i < 2; ++i)
	{
		label[5] += i;
		ret = gpio_request(devp->key[i], label);
		if (ret < 0)
		{
			PRINT("gpio request failed. label: %s, index: %d", label, devp->key[i]);
			break;
		}
	}

	if (i != 2)
	{
		for (; i >= 0; --i)
		{
			gpio_free(devp->key[i]);
		}
		return ret;
	}


	return ret;
}

static void key_request_resource(struct key_dev *devp)
{
	int i = 0;
	for (; i < 2; ++i)
	{
		gpio_free(devp->key[i]);
	}

}


static int key_configure_init(struct key_dev *devp)
{

}


static void key_configure_deinit(struct key_dev *devp)
{

}

static irqreturn_t key1_intereupt(int irq, void *dev_id)
{

}


static irqreturn_t key2_intereupt(int irq, void *dev_id)
{
	
}



static int key_probe(struct platform_device *devp)
{

}