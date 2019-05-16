KERNEL_DIR = /usr/src/linux-headers-$(shell uname -r)/

obj-m += raspberry_gpio.o

build:
	make -C $(KERNEL_DIR) M=$(CURDIR) module
clean:
	make -C $(KERNEL_DIR) M=$(CURDIR) clean
