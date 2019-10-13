
KERNEL_DIR := /home/ex/Sources/ITOP-4412
OUTPUT_DIR := $(CURDIR)

obj-m := hello_device.o
obj-m += hello_driver.o

modules:
	make -C $(KERNEL_DIR) M=$(OUTPUT_DIR) modules

clean:
	make -C $(KERNEL_DIR) M=$(OUTPUT_DIR) clean
