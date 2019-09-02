#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/platform_device.h>

#undef DEVICE_NAME
#define DEVICE_NAME "key"

#define PRINT(fmt, args...) printk(KERN_INFO "%s: " fmt, DEVICE_NAME, ##args)

struct platform_device key_platform_device =
{
	.name = DEVICE_NAME,
	.id = -1,
};

static int __init key_init(void)
{
	int ret = 0;
	ret = platform_device_register(&key_platform_device);

	if (ret < 0)
	{
		PRINT("platform device register failed");
		return ret;
	}
	return ret;
}

module_init(key_init);


static void __exit key_exit(void)
{
	platform_device_unregister(&key_platform_device);
}

module_init(key_exit);

MODULE_LICENSE("GPL v2");
