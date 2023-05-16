MOD := kerneltimer_dev
obj-m := $(MOD).o
PWD := $(shell pwd)

#KDIR := /lib/modules/$(shell uname -r)/build
KDIR := /home/ubuntu/udoo_linux_bsp/kernel/3.14.1
ARCH = ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules $(ARCH)
	cp $(MOD).ko /srv/nfs

clean:
	rm -rf *.ko
	rm -rf *.o
	rm -rf .*.cmd
	rm -rf .tmp_*
	rm -rf *.mod.*
	rm -rf Module.*
	rm -rf modules.*
