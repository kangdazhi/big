# Make the Linux ethernet bonding driver.

obj-m := bonding.o
bonding-objs := bond_main.o bond_3ad.o bond_alb.o bond_sysfs.o bond_sysfs_slave.o bond_debugfs.o bond_netlink.o bond_options.o
proc-y := bond_procfs.o
bonding-objs += $(proc-y)
