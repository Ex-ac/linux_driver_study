

ifneq ($(KERNELRELEASE), )

obj-m := faulty.o

else

KERNELRELEASE ?= /usr/src/linux-headers-$(shell uname -r)/
SOURCEPATH := $(CURDIR)

modules:
	$(MAKE) -C $(KERNELRELEASE) M=$(SOURCEPATH) modules

endif

clean:
	$(MAKE) -C $(KERNELRELEASE) M=$(SOURCEPATH) clean

 