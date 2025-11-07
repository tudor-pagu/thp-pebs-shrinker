obj-m += thp_pebs_shrinker.o

KDIR := ${HOME}/TUDelft/EPFL/Advanced_OS/linux/
PWD = $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

