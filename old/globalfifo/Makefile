KVERS = $(shell uname -r)

obj-m += globalfifo.o


build: kernel_modules

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules
	gcc main.c -o main
clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	rm ./main