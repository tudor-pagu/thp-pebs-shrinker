# OUR TOP LEVEL MAKEFILE THAT WE CAN USE TO EASILY AUTOMATE STUFF AROUND BUILDLING AND RUNNING OUR KERNEL
# IDEALLY, EVERYTHING WE WANT TO DO IS JUST ONE MAKE COMMAND
#
# DOCS:
# make build - will just build the kernel and modules
# maek install - will install the kernel 
.PHONY: copy-config build install
	
linux/.config: .config
	cp $< $@

build: copy-config
	cd linux && make -j20

install: build
	cd linux && sudo make install && sudo make modules_install && sudo update-grub
