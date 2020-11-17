KERNEL_DIR ?= /lib/modules/`uname -r`/build

obj-m = tmod_enc.o
tmod_enc-y = tmod_buff.o tmod_cdev.o tmod_worker.o tmod.o

all:
	make -C $(KERNEL_DIR) M=`pwd` modules

clean:
	make -C $(KERNEL_DIR) M=`pwd` clean
