KERNELRELEASE := /lib/modules/$(shell uname -r)/build
SOURCEPATH = $(CURDIR)

obj-m := jit.o

build:
	make -C $(KERNELRELEASE) M=$(SOURCEPATH) modules

clean:
	make -C $(KERNELRELEASE) M=$(SOURCEPATH) clean