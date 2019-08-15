Big
---
Backend support for the baremetal VM.

Compilation Requirement
-----------------------
The out-of-tree KVM compilation requires the following changes
in order to use the trace points. This is the restriction
imposed by TRACE_INCLUDE_PATH. Full pathname for out-of-tree
modules must be used.
- x86-kvm/trace.h
  #define TRACE_INCLUDE_PATH /path/to/big/x86-kvm
- x86-kvm/mmutrace.h
  #define TRACE_INCLUDE_PATH /path/to/big/x86-kvm

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
