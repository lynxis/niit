#
# Makefile for the linux squashfs routines.
#

CONFIG_SFIIT=m


obj-$(CONFIG_SFIIT) += niit.o

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
