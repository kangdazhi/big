# Compile all modules.

BOND_DIR := bonding-driver
DID_DIR := did

obj-m := big.o
bonding-objs := $(BOND_DIR)/bond_main.o \
                $(BOND_DIR)/bond_3ad.o \
                $(BOND_DIR)/bond_alb.o \
                $(BOND_DIR)/bond_sysfs.o \
                $(BOND_DIR)/bond_sysfs_slave.o \
                $(BOND_DIR)/bond_debugfs.o \
                $(BOND_DIR)/bond_netlink.o \
                $(BOND_DIR)/bond_options.o \
                $(BOND_DIR)/bond_procfs.o
did-objs := $(DID_DIR)/did.o
big-objs := $(bonding-objs) $(did-objs)

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

all: modules run_did

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insert:
	insmod big.ko dipi=1 mode=1 fail_over_mac=1

remove:
	rmmod big

run_did: $(DID_DIR)/run_did.c $(DID_DIR)/did.h
	gcc -Wall -o $(DID_DIR)/run_did $(DID_DIR)/run_did.c

.PHONY: clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm $(DID_DIR)/run_did
