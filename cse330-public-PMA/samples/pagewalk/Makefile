obj-m += pagewalk.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


install:
	sudo insmod pagewalk.ko

remove: 
	sudo rmmod pagewalk