obj-m	+= wait_event.o

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

EXTRA_CFLAGS	+= -I$(src)/../../include

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


