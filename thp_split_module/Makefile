obj-m += thp_split.o

KDIR := /home/advos1/thp-pebs-shrinker/linux
ccflags-y := -Wno-error=unused-function

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
