#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/stat.h>
#include <linux/slab.h>

#include <linux/fs.h>

#include <linux/kdev_t.h>

#include <linux/cdev.h>

#include <linux/platform_device.h>
#include <linux/device.h>



#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-exynos4.h>

#define DEVICE_NAME "char_node"
#define DRIVER_NAME "char_node"

int major = 0;
module_param(major, int, S_IRUGO);

int minor = 0;
module_param(minor, int, S_IRUGO);

struct class_dev *global_devp;
struct class *my_class;

#define PRINT(fmt, args...) printk(KERN_EMERG "class_node: " fmt, ##args)

struct class_dev
{
	int leds[2];

	struct cdev cdev;
};



static int class_open(struct inode *inode, struct file *filp)
{
	PRINT("open");
	filp->private_data = global_devp;
	return 0;
}

static int class_release(struct inode *inode, struct file *filp)
{
	PRINT("release");
	return 0;
}

static ssize_t class_read(struct file *filp, char __user *buf, size_t size, loff_t *pos)
{
	PRINT("read");
	return 0;
}

static ssize_t class_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos)
{
	int index, value;
	struct class_dev *devp = filp->private_data;

	index = buf[0] - '0';
	value = buf[2] > '0' ? 1 : 0;

	if (index > 2)
	{
		PRINT("write: args error");
		return -1;
	}

	PRINT("write: index: %2d, value: %2d", index, value);
	gpio_set_value(devp->leds[index], value);

	return size;
}

static long class_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	long ret = 0;
	struct class_dev *devp = filp->private_data;

	PRINT("ioctl: index: %2d, value: %2d", cmd, args);
	gpio_set_value(devp->leds[cmd], args);

	return ret;
}

static loff_t class_llseek(struct file *filp, loff_t offset, int where)
{
	return 0;
}

static struct file_operations class_fops =
	{
		.owner = THIS_MODULE,
		.open = class_open,
		.release = class_release,
		.read = class_read,
		.write = class_write,
		.unlocked_ioctl = class_ioctl,
		.llseek = class_llseek,
};

static int class_request_resource(struct class_dev *devp)
{
	int ret = 0;
	int i;
	int pin_size = 2;
	char label[] = "led-0";

	PRINT("start request resource");

	for (i = 0; i < pin_size; ++i)
	{
		
		label[5] += i;

		ret = gpio_request(devp->leds[i], label);

		if (ret < 0)
		{
			PRINT("request gpio failed. gpio index: %d", devp->leds[i]);
			i --;
			for (; i >= 0; --i)
			{
				gpio_free(devp->leds[i]);
			}
			return ret;
		}
		PRINT("request gpio success. gpio index: %d", devp->leds[i]);
		gpio_direction_output(devp->leds[i], 0);
	}

	return ret;
}


static void class_release_resource(struct class_dev *devp)
{
	int i;
	int pin_size = 2;

	PRINT("start release resource");

	for (i = 0; i < pin_size; ++i)
	{	
		gpio_free(devp->leds[i]);
	}
}
static int class_init_setup_cdev(struct class_dev *devp, int index)
{
	int ret = 0;
	dev_t devno = MKDEV(major, index);

	PRINT("start initialized cdev. index: %d", index);

	cdev_init(&devp->cdev, &class_fops);
	devp->cdev.owner = THIS_MODULE;
	devp->cdev.ops = &class_fops;

	ret = cdev_add(&devp->cdev, devno, 1);

	if (ret)
	{
		PRINT("can't add device. No: %2d", index);
	}
	device_create(my_class, NULL, devno, NULL, DEVICE_NAME"_%d", index);
	return ret;
}


static void class_exit_setup_cdev(struct class_dev *devp, int index)
{
	dev_t devno = MKDEV(major, index);

	PRINT("start deinitialized cdev. index: %d", index);
	device_destroy(my_class, devno);

	cdev_del(&devp->cdev);
}


static int class_probe(struct platform_device *devp)
{
	int ret;
	PRINT("Initialized...");

	global_devp = (struct class_dev *)(kzalloc(sizeof(struct class_dev), GFP_KERNEL));

	global_devp->leds[0] = EXYNOS4_GPL2(0);
	global_devp->leds[1] = EXYNOS4_GPK1(1);
	
	ret = class_request_resource(global_devp);

	ret = class_init_setup_cdev(global_devp, 0);

	if (ret)
	{
		class_release_resource(global_devp);
		kfree(global_devp);
		return -1;
	}

	return 0;
}

static int class_remove(struct platform_device *devp)
{
	PRINT("remove");
	
	class_exit_setup_cdev(global_devp, 0);

	class_release_resource(global_devp);
	return 0;
}

static void class_shutdown(struct platform_device *devp)
{
	PRINT("shutdown");

}


static int class_suspend(struct platform_device *devp, pm_message_t state)
{
	PRINT("suspend");
	return 0;
}

static int class_resume(struct platform_device *devp)
{
	PRINT("resume");

	return 0;
}

static struct platform_driver class_driver = 
{
	.probe = class_probe,
	.remove = class_remove,
	.shutdown = class_shutdown,
	.suspend = class_suspend,
	.resume = class_resume,
	.driver = 
	{
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};


static int __init class_init(void)
{
	int ret = 0;
	dev_t devno = MKDEV(major, 0);


	
	if (major)
	{
		ret = register_chrdev_region(devno, 1, DEVICE_NAME);
	}
	else
	{
		ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
		major = MAJOR(devno);
	}

	if (ret < 0)
	{
		PRINT("can't register cdev number");
		return ret;
	}
	
	my_class = class_create(THIS_MODULE, DEVICE_NAME);

	ret = platform_driver_register(&class_driver);

	if (ret < 0)
	{
		PRINT("driver register failed. code: %d", ret);
		class_destroy(my_class);
		unregister_chrdev_region(devno, 1);
		return ret;
	}
	return ret;
}

module_init(class_init);

static void __exit class_exit(void)
{
	dev_t devno = MKDEV(major, minor);

	platform_driver_unregister(&class_driver);

	class_destroy(my_class);
	unregister_chrdev_region(devno, 1);
}

module_exit(class_exit);

MODULE_LICENSE("GPL v2");
