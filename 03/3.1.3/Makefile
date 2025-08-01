obj-m	+= seek.o

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -g -W -Wall seek_test.c -o seek_test
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f seek_test


