Big
---
Big module has the following components to achieve the
direct-interrupt delivery (DID) and PCI-device passthrough in
Linux 4.10.1 by VT-x and VT-d. DID includes the direct
PCI-device interrupts, timer interrupts and IPIs.
- KVM is modified to support DID.
- The DID device is included in the guest. When the guest
  needs to enable the DID, it uses the device to communicate
  with KVM.
- Additionally, the Ethernet bonding driver is included in the
  guest. This is not related to DID, but the switch of network
  traffic between the passthrough and Virtio NIC.

Source Code
-----------
- https://github.com/osnetsvn/pci-passthrough.git
- git@github.com:osnetsvn/pci-passthrough.git
- PATH: pci-passthrough/code/big.tar.gz
- Big itself is a git repository, but not a submodule of
  pci-passthrough repository.
  - List all the branches.
    # git branch
  - For the host, please use the host branch.
    # git checkout host
  - For the guest, please use the guest branch.
    # git checkout guest

Pre-Requisite
-------------
- Enable VT-x and VT-d from BIOS.
- Use Linux 4.10.1 for both the host and guest.
- Use VFIO for the directed I/O.
- Use QEMU 2.9.1 or above.
- Use the following kernel parameters in the host. For a
  better clarification, We tag the kernel parameters. The tags
  are (1) DID, (2) Recommend and (3) Option. (1) is required
  for DID and the direct PCI-device assignment. (2) is
  recommended and perhaps used currently. (3) is optional.
  - (DID)            intel_iommu=on
  - (DID)            intremap=no_x2apic_optout
  - (DID)            isolcpus=1-n
  - (DID)            lapic=notscdeadline
  - (DID, Recommend) nohz=off
  - (Recommend)      transparent_hugepage=madvise
  - (Option)         nowatchdog
  - (Option)         rcupdate.rcu_cpu_stall_suppress=1
- Use the following kernel parameters in the guest. For a
  better clarification, We tag the kernel parameters.
  - (DID)            lapic=notscdeadline
  - (DID, Recommend) nohz=off
  - (Recommend)      transparent_hugepage=madvise
  - (Option)         nowatchdog
  - (Option)         rcupdate.rcu_cpu_stall_suppress=1
  - (Option)         console=ttyS0,115200n8

Install DID-KVM in the Host
---------------------------
- Assume we are in the host branch of Big repository (big/),
  which has the following files.
  - include/
  - Makefile
  - osnet/
  - README.txt
  - virt-kvm/
  - x86-kvm/
- Make the proper changes for the out-of-tree KVM modules.
  - x86-kvm/trace.h
    #define TRACE_INCLUDE_PATH /path/to/big/x86-kvm
  - x86-kvm/mmutrace.h
    #define TRACE_INCLUDE_PATH /path/to/big/x86-kvm
- Make the modules.
  # make
- Remove the existing KVM modules.
  # make remove
- Insert the DID-KVM modules.
  # make insert

Install the DID Device in the Guest
-----------------------------------
- Assume we are in the guest branch of Big repository (big/),
  which has the following files.
  - bonding-driver/
  - did/
  - Makefile
  - README.txt
- Make the modules.
  # make
- Insert the DID device.
  # make insert
  # ls /dev/did

Dedicate PCPU to VCPU
---------------------
- Isolate the PCPUs by isolcpus.
- Boot up the QEMU/KVM guest.
- Pin each VCPU thread to its PCPU, one VCPU per PCPU.
  # taskset -cp $pcpu $vcpu

Enable Direct Timer Interrupt and IPI in the Guest
--------------------------------------------------
- Install the DID device in the guest.
- Enable direct timer and inter-processor interrupts.
  # cd big/did
  # ./did.sh enable_did

Direct PCI-Device Assignment
----------------------------
- pci-passthrough/pci_device_passthrough_tutorial.txt

QEMU Example
------------
qemu-system-x86_64 \
        -enable-kvm \
        -cpu host,host-cache-info=on \
        -smp ${vcpu},cores=${vcpu},threads=1,sockets=1 \
        -m   ${memory} \
        -drive file=${vm_image},if=virtio \
        -device vfio-pci,host=${bdf},id=pnic \
        -qmp unix:/tmp/qmp-socket,server,nowait \
        -monitor none \
        -parallel none \
        -serial none \
        -net none \
        -vga none \
        -nographic \
        -serial telnet:127.0.0.1:8888,server,nowait \
	-nodefaults
