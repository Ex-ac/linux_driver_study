#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/fcntl.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/slab.h>

#include <linux/platform_device.h>

#define NAME "Test"

#undef PRINT_DEBUG
#ifndef DEBUG
#	ifdef __KERNEL__
#		define PRINT_DEBUG(fmt, args...) printk(KERN_INFO "%s: " fmt, NAME, ##args)
#	else
#		define (fmt, args...) fprintf(KERN_INFO "%s: " fmt, NAME, ##args)
#	endif
#else
#	define PRINT_DEBUG(fmt, args...)
#endif

static int hello_probe(struct platform_device *pdev)
{
	PRINT_DEBUG("\tInitalized\n");

	PRINT_DEBUG("platform_device name: %s", pdev->name);
	PRINT_DEBUG("platform_device id: %i", pdev->id);

	pdev->dev.release(&pdev->dev);

	return 0;
}


static int hello_remove(struct platform_device *pdev)
{
	return 0;
}

static int hello_shutdown(struct platform_device *pdev)
{
	return 0;
}


static int hello_suspend(struct platform_device *pdev)
{
	return 0;
}


static int hello_resume(struct platform_device *pdev)
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
		.name = NAME,
		.owner = THIS_MODULE,
	},
};

static int __init hello_init(void)
{
	int ret;
	PRINT_DEBUG("Hello module enter");

	ret = platform_driver_register(&hello_driver);

	if (ret)
	{
		PRINT_DEBUG("driver register failed");
		return ret;
	}
	PRINT_DEBUG("driver register success %d", ret);
	return 0;
}

module_init(hello_init);

static void __exit hello_exit(void)
{
	PRINT_DEBUG("Hello module exit");

	platform_driver_unregister(&hello_driver);
}

module_exit(hello_exit);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ex-ac");
