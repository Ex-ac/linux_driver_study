ifeq ($(DEBUG),y)
	DEBUGFLAGE = -O -g 
#-DSCULL_DEBUG -D__KERNEL__
else
	DEBUGFLAGE = -O2
endif

EXTRA_CFLAGS += $(DEBUGFLAGE)

ifneq ($(KERNELRELEASE), )

obj-m := scull.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif


clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend, $(wildcard .depend))
include .depend
endif
