# Compile all modules.

did-obj := did/
bond-obj := bonding-driver/

obj-m := $(did-obj) $(bond-obj)

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

all: modules run_did

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insert:
	insmod did.ko dipi=1

remove:
	rmmod did

run_did: did/run_did.c did/did.h
	gcc -Wall -o did/run_did did/run_did.c

.PHONY: clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm did/run_did
