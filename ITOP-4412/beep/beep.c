#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/stat.h>
#include <linux/fs.h>

#include <linux/slab.h>

#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-exynos4.h>

#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/device.h>

#include <linux/io.h>
#include <linux/ioport.h>
#include <asm/io.h>

#include <linux/types.h>

#undef DEVICE_NAME
#define DEVICE_NAME "beep"

#define PWM_PYSICS_ADDRESS (0x139D0000)
#define PWM_PYSICS_SIZE (0x32)

int major = 0;
module_param(major, int, S_IRUGO);

struct beep_dev
{
	int pin;

	struct cdev cdev;

	uint32_t pwm_phyics_address;
	uint32_t pwm_register_size;
	struct pwm_register *pwm_register;
};

struct pwm_register
{
	uint32_t TCFG0;
	uint32_t TCFG1;

	uint32_t TCON;

	uint32_t TCNTB0;
	uint32_t TCMPB0;
	uint32_t TCNTO0;

	uint32_t TCNTB1;
	uint32_t TCMPB1;
	uint32_t TCNTO1;

	uint32_t TCNTB2;
	uint32_t TCMPB2;
	uint32_t TCNTO2;

	uint32_t TCNTB3;
	uint32_t TCMPB3;
	uint32_t TCNTO3;

	uint32_t TCNTB4;
	uint32_t TCNTO4;

	uint32_t TINT_CSTAT;
};

struct class *global_class;
struct beep_dev *global_beep_devp;

#define PRINT(fmt, args...) printk(KERN_EMERG "%s: " fmt, DEVICE_NAME, ##args)

static int beep_open(struct inode *inode, struct file *filp);
static int beep_release(struct inode *inode, struct file *filp);
static ssize_t beep_read(struct file *filp, char __user *buf, size_t size, loff_t *pos);
static ssize_t beep_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos);

static long beep_ioctl(struct file *filp, unsigned int cmd, unsigned long args);
static loff_t beep_llseek(struct file *filp, loff_t offset, int whence);

static int beep_resource_request(struct beep_dev *devp)
{
	int ret = 0;
	struct resource *resource;

	PRINT("%s", __func__);
	ret = gpio_request(devp->pin, DEVICE_NAME);

	if (ret < 0)
	{
		PRINT("gpio request failed. gpio pin: %d", devp->pin);
		return ret;
	}

	resource = request_mem_region(devp->pwm_phyics_address, devp->pwm_register_size, DEVICE_NAME "_PWM");

	if (resource == NULL)
	{
		PRINT("request memory region failed. start address: 0x%x, size: %d", devp->pwm_phyics_address, devp->pwm_register_size);

		gpio_free(devp->pin);

		return -1;
	}

	devp->pwm_register = (struct pwm_register *)(ioremap(devp->pwm_phyics_address, devp->pwm_register_size));

	if (devp->pwm_register == NULL)
	{
		PRINT("ioremap failed. pyhsics address: 0x%d, size: %d", devp->pwm_phyics_address, devp->pwm_register_size);

		gpio_free(devp->pin);
		return -1;
	}

	return ret;
}

static void beep_resource_free(struct beep_dev *devp)
{
	PRINT("%s", __func__);
	gpio_free(devp->pin);
	iounmap(devp->pwm_register);
	release_mem_region(devp->pwm_phyics_address, devp->pwm_register_size);
}

static int beep_configure_init(struct beep_dev *devp)
{
	int ret = 0;
	uint32_t temp;
	int i = 0;

	PRINT("%s", __func__);

	ret = beep_resource_request(devp);

	if (ret < 0)
	{
		return ret;
	}

	ret = s3c_gpio_cfgpin(devp->pin, S3C_GPIO_SFN(2));

	if (ret < 0)
	{
		PRINT("gpio configure output TOUT_0 failed.");

		gpio_free(devp->pin);

		beep_resource_free(devp);
		return ret;
	}

	// configure beep registers

	//设置分频数
	barrier();
	PRINT("%s, %d", __func__, i ++);
	temp = ioread32(&(devp->pwm_register->TCFG0));
	temp &= (~0xff);
	temp |= 0xf9;

	PRINT("%s, %d", __func__, i ++);
	iowrite32(temp, &(devp->pwm_register->TCFG0));

	PRINT("%s, %d", __func__, i ++);
	temp = ioread32(&(devp->pwm_register->TCFG1));
	temp &= (~0x4);
	temp |= 0x2;

	PRINT("%s, %d", __func__, i ++);
	iowrite32(temp, &(devp->pwm_register->TCFG1));

	PRINT("%s, %d", __func__, i ++);
	iowrite32(50, &(devp->pwm_register->TCMPB0));

	PRINT("%s, %d", __func__, i ++);
	iowrite32(100, &(devp->pwm_register->TCNTB0));

	PRINT("%s, %d", __func__, i ++);
	temp = ioread32(&(devp->pwm_register->TCON));

	temp |= (0x01 + 0x02);
	
	PRINT("%s, %d", __func__, i ++);
	iowrite32(temp, &(devp->pwm_register->TCON));
	
	PRINT("%s, %d", __func__, i ++);
	barrier();
}

static void beep_configure_deinit(struct beep_dev *devp)
{
	// reset configure
	PRINT("%s", __func__);
	beep_resource_free(devp);
}

struct file_operations beep_fops =
{
		.owner = THIS_MODULE,
		.open = beep_open,
		.release = beep_release,
		.read = beep_read,
		.write = beep_write,
		.llseek = beep_llseek,
		.unlocked_ioctl = beep_ioctl,
};

static int beep_setup_init(struct beep_dev *devp, int index)
{
	int ret = 0;
	dev_t devno = MKDEV(major, index);

	PRINT("%s", __func__);
	ret = beep_configure_init(devp);

	if (ret < 0)
	{
		PRINT("beep init failed");
		return ret;
	}

	cdev_init(&devp->cdev, &beep_fops);
	devp->cdev.owner = THIS_MODULE;

	ret = cdev_add(&devp->cdev, devno, 1);

	if (ret < 0)
	{
		PRINT("can't add device, index: %ld, error code: %ld", index, ret);

		beep_configure_deinit(devp);
		return ret;
	}

	device_create(global_class, NULL, devno, NULL, DEVICE_NAME "_%2d", index);
	return ret;
}

static void beep_setup_deinit(struct beep_dev *devp, int index)
{
	dev_t devno = MKDEV(major, index);

	PRINT("%s", __func__);
	device_destroy(global_class, devno);

	cdev_del(&devp->cdev);

	beep_configure_deinit(devp);
}

static int __init beep_init(void)
{
	int ret = 0;
	dev_t devno = MKDEV(major, 0);

	PRINT("%s", __func__);

	if (major > 0)
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

	global_class = class_create(THIS_MODULE, DEVICE_NAME);

	if (global_class == NULL)
	{
		PRINT("can't create class: %s", DEVICE_NAME);
		unregister_chrdev_region(devno, 1);
		return -1;
	}

	global_beep_devp = (struct beep_dev *)(kzalloc(sizeof(struct beep_dev), GFP_KERNEL));

	if (global_beep_devp == NULL)
	{
		PRINT("memory not enought");
		class_destroy(global_class);
		unregister_chrdev_region(devno, 1);
		return -ENOMEM;
	}

	global_beep_devp->pin = EXYNOS4_GPD0(0);
	global_beep_devp->pwm_phyics_address = PWM_PYSICS_ADDRESS;
	global_beep_devp->pwm_register_size = PWM_PYSICS_SIZE;

	ret = beep_setup_init(global_beep_devp, 0);

	if (ret < 0)
	{
		class_destroy(global_class);
		kfree(global_beep_devp);
		unregister_chrdev_region(devno, 1);
	}

	return ret;
}

module_init(beep_init);

static void __exit beep_exit(void)
{
	dev_t devno = MKDEV(major, 0);

	PRINT("%s", __func__);

	beep_setup_deinit(global_beep_devp, 0);

	class_destroy(global_class);
	kfree(global_beep_devp);
	unregister_chrdev_region(devno, 1);
}
module_exit(beep_exit);

static void beep_on(struct beep_dev *devp)
{
	uint32_t temp;

	PRINT("%s", __func__);

	barrier();

	temp = ioread32(&(devp->pwm_register->TCON));
	temp &= (~0x0f);
	temp |= (0x01 | 0x08);

	iowrite32(temp, &(devp->pwm_register->TCON));
	barrier();
}

static void beep_off(struct beep_dev *devp)
{
	uint32_t temp;

	PRINT("%s", __func__);

	barrier();

	temp = ioread32(&(devp->pwm_register->TCON));
	temp &= (~0x0f);

	iowrite32(temp, &(devp->pwm_register->TCON));
	barrier();
}

static int beep_open(struct inode *inode, struct file *filp)
{	

	PRINT("%s", __func__);
	filp->private_data = container_of(inode, struct beep_dev, cdev);

	return 0;
}

static int beep_release(struct inode *inode, struct file *filp)
{
	PRINT("%s", __func__);
	return 0;
}

static ssize_t beep_write(struct file *filp, const char __user *buf, size_t size, loff_t *pos)
{
	struct beep_dev *devp = container_of(filp->private_data, struct beep_dev, cdev);

	PRINT("%s", __func__);
	
	if (buf[0] > 0)
	{
		beep_on(devp);
	}
	else
	{
		beep_off(devp);
	}

	return size;
}

static ssize_t beep_read(struct file *filp, char __user *buf, size_t size, loff_t *pos)
{

	PRINT("%s", __func__);

	return 0;
}

static long beep_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{

	struct beep_dev *devp = container_of(filp->private_data, struct beep_dev, cdev);

	PRINT("%s", __func__);

	if (cmd == 0)
	{
		beep_on(devp);
	}
	else
	{
		beep_off(devp);
	}

	return 0;
}

static loff_t beep_llseek(struct file *filp, loff_t offset, int whence)
{
	PRINT("%s", __func__);

	return 0;
}

MODULE_LICENSE("GPL v2");