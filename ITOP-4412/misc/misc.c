#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/platform_device.h>

#include <linux/miscdevice.h>

#include <linux/fs.h>

#include <linux/kernel.h>

#define DRIVER_NAME "hello_ctl"
#define DEVICE_NAME "hello_ctl123"

#define PRINT(fmt, args...) printk(KERN_INFO "hello misc: " fmt, ##args)

static long hello_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	PRINT("cmd: %d, arg: %d", cmd, arg);
}

static int hello_open(struct inode *inode, struct file *filp)
{
	PRINT("%s", __func__);
	return 0;
}

static int hello_release(struct inode *inode, struct file *filp)
{
	PRINT("%s", __func__);
	return 0;
}


static ssize_t hello_read(struct file *filp, char __user *buf, size_t size, loff_t *pos)
{
	PRINT("%s", __func__);
	return 0;
}
static struct file_operations hello_ops =
	{
		.owner = THIS_MODULE,
		.open = hello_open,
		.release = hello_release,
		.read = hello_read,
		.unlocked_ioctl = hello_ioctl,
};

static struct miscdevice hello_dev =
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEVICE_NAME,
		.fops = &hello_ops,
};

static int hello_probe(struct platform_device *devp)
{
	PRINT("Initialized");
	misc_register(&hello_dev);

	return 0;
}

static int hello_remove(struct platform_device *devp)
{
	PRINT("Remove");
	misc_deregister(&hello_dev);
	return 0;
}

static void hello_shutdown(struct platform_device *pdv)
{

}

static int hello_suspend(struct platform_device *pdv, pm_message_t pmt)
{

	return 0;
}

static int hello_resume(struct platform_device *pdv)
{
	return 0;
}

static struct platform_driver hello_driver = 
{
	.probe = hello_probe,
	.remove = hello_remove,
	.shutdown = hello_shutdown,
	.suspend = hello_suspend,
	.resume = hello_resume,
	.driver =
	{
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init hello_init(void)
{
	int ret;

	PRINT("modele enter");

	ret = platform_driver_register(&hello_driver);

	if (ret)
	{
		PRINT("driver register failed");
		return ret;
	}

	PRINT("driver register success");
	return 0;
}

module_init(hello_init);


static void __exit hello_exit(void)
{
	PRINT("modele exit");

	platform_driver_unregister(&hello_driver);
}

module_exit(hello_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ex-ac");