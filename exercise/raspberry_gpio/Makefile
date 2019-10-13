CONFIG_MODULE_SIG_ALL=y 
KERNEL_DIR = /usr/src/linux-headers-$(shell uname -r)/

obj-m += raspberry_gpio.o

build:
	make -C $(KERNEL_DIR) M=$(CURDIR) modules
clean:
	make -C $(KERNEL_DIR) M=$(CURDIR) clean
