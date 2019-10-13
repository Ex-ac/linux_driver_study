KERNEL_DIR = $(SOURCES)/ITOP-4412/

obj-m := gpio_remap.o

modules:
	make -C $(KERNEL_DIR) M=$(CURDIR) modules

clean:
	make -C $(KERNEL_DIR) M=$(CURDIR) clean
	