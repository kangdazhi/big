# Compile kvm modules.

VIRT-KVM := virt-kvm
VIRT-KVM-LIB := $(VIRT-KVM)/lib
X86-KVM := x86-kvm

kvm-y += $(VIRT-KVM)/kvm_main.o $(VIRT-KVM)/coalesced_mmio.o \
         $(VIRT-KVM)/eventfd.o $(VIRT-KVM)/irqchip.o $(VIRT-KVM)/vfio.o
kvm-$(CONFIG_KVM_ASYNC_PF) += $(VIRT-KVM)/async_pf.o
kvm-y += $(X86-KVM)/x86.o $(X86-KVM)/mmu.o $(X86-KVM)/emulate.o \
         $(X86-KVM)/i8259.o $(X86-KVM)/irq.o $(X86-KVM)/lapic.o \
         $(X86-KVM)/i8254.o $(X86-KVM)/ioapic.o $(X86-KVM)/irq_comm.o \
         $(X86-KVM)/cpuid.o $(X86-KVM)/pmu.o $(X86-KVM)/mtrr.o \
         $(X86-KVM)/hyperv.o $(X86-KVM)/page_track.o $(X86-KVM)/debugfs.o
kvm-$(CONFIG_KVM_DEVICE_ASSIGNMENT) += assigned-dev.o iommu.o
kvm-intel-y += $(X86-KVM)/vmx.o $(X86-KVM)/pmu_intel.o
irqbypass-y += $(VIRT-KVM-LIB)/irqbypass.o

obj-$(CONFIG_KVM) += kvm.o
obj-$(CONFIG_KVM_INTEL) += kvm-intel.o
obj-$(CONFIG_IRQ_BYPASS_MANAGER) += irqbypass.o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

all: modules

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insert:
	insmod irqbypass.ko
	insmod kvm.ko halt_poll_ns=0
	insmod kvm-intel.ko preemption_timer=0

remove:
	rmmod kvm-intel.ko
	rmmod kvm.ko
	rmmod irqbypass.ko

.PHONY: clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
