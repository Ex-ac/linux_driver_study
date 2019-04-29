KERNELRELEASE := /usr/src/linux-headers-$(shell uname -r)/
SOURCEPATH = $(CURDIR)

obj-m := jit.o

build:
	make -C $(KERNELRELEASE) M=$(SOURCEPATH) modules

clean:
	make -C $(KERNELRELEASE) M=$(SOURCEPATH) clean