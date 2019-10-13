# ifeq ($(DEBUG), y)
# 	DEBUG_FLAGE = -O -g -DSCULLC_DEBUG
# else
# 	DEBUG_FLAGE = -O2
# endif

# EXTRA_CFLAGES += $(DEBUG_FLAGE)

# ifneq ($(KERNEL_RELEASE), )
# obj-m := scullc.o

# else

# KERNEL_RELEASE ?= /usr/src/linux-headers-$(shell uname -r)/

# modules:
# 	$(MAKE) -C $(KERNEL_RELEASE) M=$(CURDIR) modules
# endif

# clean:
# 	$(MAKE) -C $(KERNEL_RELEASE) M=$(CURDIR) clean

# depend .depend dep:
# 	$(CC) $(EXTRA_CFLAGES) -M *.c > .depend

# ifeq (.depend, $(wildcard .depend))
# include .depend
# endif

KERNEK_DIR = /usr/src/linux-headers-$(shell uname -r)/

obj-m += scullc.o


build:
	make -C $(KERNEK_DIR) M=$(CURDIR) modules
clean:
	make -C $(KERNEK_DIR) M=$(CURDIR) clean
