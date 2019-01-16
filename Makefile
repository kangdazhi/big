obj-m += did.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

all: modules run_did

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insert:
	insmod did.ko

remove:
	rmmod did

run_did: run_did.c did.h
	gcc -Wall -o run_did run_did.c

.PHONY: clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm run_did
