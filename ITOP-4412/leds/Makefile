OUTPUT_DIR := $(CURDIR)
KERNEL_DIR:= /home/ex/Sources/ITOP-4412/

obj-m := leds.o

modules:
	make -C $(KERNEL_DIR) M=$(OUTPUT_DIR) modules

clean:
	make -C $(KERNEL_DIR) M=$(OUTPUT_DIR) clean

test: test.o test2.o
	gcc -o test.out test.o test2.o
	
test.o test2.o: test.c test2.c
	gcc -c test.c
	gcc -c test2.c

clean-test:
	rm -rf test.o
	rm -rf test2.o
	rm -rf test.out