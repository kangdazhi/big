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
  Ex: #define TRACE_INCLUDE_PATH /home/tcheng8/kvm/x86-kvm
- x86-kvm/mmutrace.h
  Ex: #define TRACE_INCLUDE_PATH /home/tcheng8/kvm/x86-kvm
