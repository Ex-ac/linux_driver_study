KERNEL_DIR = $(SOURCES)/ITOP-4412/

obj-m := beep.o


modules:
	make -C $(KERNEL_DIR) M=$(CURDIR) modules

clean:
	make -C $(KERNEL_DIR) M=$(CURDIR) clean
	