obj-m += thp_split.o

KDIR := /usr/src/linux-6.15.0
ccflags-y := -Wno-error=unused-function

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
