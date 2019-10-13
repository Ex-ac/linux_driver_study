KERNEL_DIR := /home/ex/Sources/ITOP-4412

obj-m := misc.o


modules:
	make -C $(KERNEL_DIR) M=$(CURDIR) modules

clean:
	make -C $(KERNEL_DIR) M=$(CURDIR) clean
