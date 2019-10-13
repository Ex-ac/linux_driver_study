#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/platform_device.h>

#include <linux/miscdevice.h>

#include <linux/fs.h>

#include <linux/gpio.h>

#include <plat/gpio-cfg.h>
#include <mach/gpio.h>

#include <mach/gpio-exynos4.h>

#define DRIVER_NAME		"Test"
#define DEVICE_NAME		"Test123"

#define LED_PIN			EXYNOS4_GPL2(0)

#define PRINT(fmt, args...) printk(KERN_EMERG "leds: " fmt, ##args)



static int leds_open(struct inode *inode, struct file *filp);
static int leds_release(struct inode *inode, struct file *filp);

static ssize_t leds_read(struct file *filp, char __user * buf, size_t size, loff_t *ops);
static ssize_t leds_write(struct file *filp, const char __user * buf, size_t size, loff_t *ops);

static long leds_ioctl(struct file *filp, unsigned int cmd, unsigned long args);


static struct file_operations leds_fops = 
{
	.owner = THIS_MODULE,
	.open = leds_open,
	.release = leds_release,
	.read = leds_read,
	.write = leds_write,
	.unlocked_ioctl = leds_ioctl,
};

static struct miscdevice leds_miscdevice =
{
	.name = DEVICE_NAME,
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &leds_fops,
};

// 平台驱动，初始化探测，移除，关闭，停止，恢复
static int leds_probe(struct platform_device *devp)
{
	int ret;
	PRINT("Initialized...");

	ret = gpio_request(LED_PIN, "led");

	if (ret < 0)
	{
		PRINT("request gpio failed. gpio index: %d", LED_PIN);
		return ret;
	}

	s3c_gpio_cfgpin(LED_PIN, S3C_GPIO_OUTPUT);
	
	gpio_set_value(LED_PIN, 0);
	misc_register(&leds_miscdevice);

	return 0;
}

static int leds_remove(struct platform_device *devp)
{
	PRINT("remove");
	gpio_free(LED_PIN);
	misc_deregister(&leds_miscdevice);
	return 0;
}


static void leds_shutdown(struct platform_device *devp)
{
	PRINT("shutdown");
}

static int leds_suspend(struct platform_device *devp, pm_message_t state)
{
	PRINT("suspend...");
	return 0;
}


static int leds_resume(struct platform_device *devp)
{
	PRINT("resume");

	return 0;
}

int leds_open(struct inode *inode, struct file *filp)
{
	PRINT("open");
	return 0;
}

int leds_release(struct inode *inode, struct file *filp)
{
	PRINT("release");
	return 0;
}


ssize_t leds_read(struct file *filp, char __user *buf, size_t size, loff_t *pos)
{
	ssize_t ret;
	int value = gpio_get_value(LED_PIN);

	PRINT("read");
	ret = sprintf(buf, "current led values is %s\n", value ? "height" : "low");

	return ret;
}

ssize_t leds_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos)
{
	
	int value = (buf[0] - '0') >  0;

	PRINT("write");
	gpio_set_value(LED_PIN, value);

	return size;
}

long leds_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	PRINT("ioctl");

	if (cmd > 0)
	{
		PRINT("gpio will set height");
		gpio_set_value(LED_PIN, 1);
	}
	else
	{
		PRINT("gpio will set low");
		gpio_set_value(LED_PIN, 0);
	}
	return 0;
}


static struct platform_driver leds_driver = 
{
	.probe = leds_probe,
	.remove = leds_remove,
	.shutdown = leds_shutdown,
	.suspend = leds_suspend,
	.resume = leds_resume,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
	},
};

static int __init leds_init(void)
{
	int ret = 0;

	PRINT("leds module enter");

	ret = platform_driver_register(&leds_driver);

	return ret;
}

module_init(leds_init);

static void __exit leds_exit(void)
{
	PRINT("leds module exit");

	platform_driver_unregister(&leds_driver);
}

module_exit(leds_exit);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ex-ac");