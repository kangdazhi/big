#if 0
#ifndef _UAPI__LINUX_KVM_PARA_H
#define _UAPI__LINUX_KVM_PARA_H

/*
 * This header file provides a method for making a hypercall to the host
 * Architectures should define:
 * - kvm_hypercall0, kvm_hypercall1...
 * - kvm_arch_para_features
 * - kvm_para_available
 */

/* Return values for hypercalls */
#define KVM_ENOSYS		1000
#define KVM_EFAULT		EFAULT
#define KVM_E2BIG		E2BIG
#define KVM_EPERM		EPERM

#define KVM_HC_VAPIC_POLL_IRQ		1
#define KVM_HC_MMU_OP			2
#define KVM_HC_FEATURES			3
#define KVM_HC_PPC_MAP_MAGIC_PAGE	4
#define KVM_HC_KICK_CPU			5
#define KVM_HC_MIPS_GET_CLOCK_FREQ	6
#define KVM_HC_MIPS_EXIT_VM		7
#define KVM_HC_MIPS_CONSOLE_OUTPUT	8

/*
 * hypercalls use architecture specific
 */
#include <asm/kvm_para.h>

#endif /* _UAPI__LINUX_KVM_PARA_H */
#endif

/* OSNET hypercalls */
#ifndef _OSNET_UAPI_ADDITIONAL_HC_H
#define _OSNET_UAPI_ADDITIONAL_HC_H

#define KVM_HC_SETUP_DTID                       100
#define KVM_HC_RESTORE_DTID                     101
#define KVM_HC_MAP_PID                          200
#define KVM_HC_UNMAP_PID                        201
#define KVM_HC_SET_X2APIC_ID                    300
#define KVM_HC_RESTORE_X2APIC_ID                301
#define KVM_HC_DISABLE_INTERCEPT_WRMSR_ICR      302
#define KVM_HC_ENABLE_INTERCEPT_WRMSR_ICR       303
#define KVM_HC_SET_CPU_EXEC_VMCS                304
#define KVM_HC_RESTORE_CPU_EXEC_VMCS            305
#define KVM_HC_DUMP_VMCS                        306
#define KVM_HC_SET_PIN_BASED_EXEC_VMCS          307
#define KVM_HC_SET_SECONDARY_EXEC_VMCS          308
#define KVM_HC_SET_EXCEPTION_BITMAP             309
#define KVM_HC_GET_CLOCKEVENT_MULT              400
#define KVM_HC_GET_CLOCKEVENT_SHIFT             401
#define KVM_HC_TEST                             500
#define KVM_HC_PAGE_WALK                        501

#endif  /* _OSNET_UAPI_ADDITIONAL_HC_H */
