#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/fcntl.h>
#include <linux/slab.h>

#include <linux/platform_device.h>

#define NAME "char_node"

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


static void hello_release(struct device *dev)
{
	PRINT_DEBUG("%s", __func__);
};

static struct platform_device hello_device =
{
	.name = NAME,
	.id = -1,
	.dev = 
	{
		.release = hello_release,
	},
};


static int __init hello_init(void)
{
	int ret;
	ret = platform_device_register(&hello_device);
	
	if (ret)
	{
		PRINT_DEBUG("device register failed");
		return ret;
	}
	PRINT_DEBUG("device register success. %d", ret);
	return ret;
}


static void __exit hello_exit(void)
{
	platform_device_unregister(&hello_device);
}

module_exit(hello_exit);
module_init(hello_init);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ex-ac");
