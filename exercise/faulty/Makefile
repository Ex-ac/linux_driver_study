

ifneq ($(KERNELRELEASE), )

obj-m := faulty.o

else

KERNELRELEASE ?= /lib/modules/$(shell uname -r)/build
SOURCEPATH := $(CURDIR)

modules:
	$(MAKE) -C $(KERNELRELEASE) M=$(SOURCEPATH) modules

endif

clean:
	$(MAKE) -C $(KERNELRELEASE) M=$(SOURCEPATH) clean

 