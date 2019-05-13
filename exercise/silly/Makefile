KERNEK_DIR = /usr/src/linux-headers-$(shell uname -r)/

obj-m += silly.o

build:
	make -C $(KERNEK_DIR) M=$(CURDIR) modules

clean:
	make -C $(KERNEK_DIR) M=$(CURDIR) clean