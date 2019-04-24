ifeq ($(DEBUG), y)
	DEBUGFLAG = -O -g -DSCULLPIPE_DEBUG -D__KERNEL__
else
	DEBUGFLAG = -O2
endif

EXTRA_CFLAGES += $(DEBUGFLAG)

ifneq ($(KERNELRELEASE), )
obj-m := scullpipe.o
else
KERNELRELEASE ?= /lib/modules/$(shell uname -r)/build
SOURCEPATH := $(CURDIR)/

modules:
	$(MAKE) -C $(KERNELRELEASE) M=$(SOURCEPATH) modules
endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .temp_versions

depend .depend dep:
	$(CC) %(EXTRA_CFLAGES) -M *.c > .depend

ifeq (.depend, $(wildcard .depend))
include .depend
endif