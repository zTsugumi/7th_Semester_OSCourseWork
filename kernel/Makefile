ifneq ($(KERNELRELEASE),)
	obj-m += my_vdev.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions *.symvers *.order *.mod
