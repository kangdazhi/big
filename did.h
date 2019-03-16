/*
 * Direct interrupt delivery
 */
#ifndef _DID_H
#define _DID_H

#define DEVICE_NAME "did"
#define DEVICE "/dev/" DEVICE_NAME

/* clockevent device */
typedef struct clockevent_device_t {
        char name[10];
        unsigned int mult;
        unsigned int shift;
} clockevent_device_t;

/* hypercall */
#define KVM_HC_SETUP_DTID       100
#define KVM_HC_RESTORE_DTID     101
#define KVM_HC_MAP_PID          200
#define KVM_HC_UNMAP_PID        201
#define KVM_HC_PAGE_WALK        202
#define KVM_HC_SET_X2APIC_ID                    300
#define KVM_HC_RESTORE_X2APIC_ID                301
#define KVM_HC_DISABLE_INTERCEPT_WRMSR_ICR      302
#define KVM_HC_ENABLE_INTERCEPT_WRMSR_ICR       303

/* ioctl to hypercall */
#define HC_MAP_PID                      _IO('k', 1000)
#define HC_UNMAP_PID                    _IO('k', 1001)
#define HC_PAGE_WALK                    _IO('k', 1002)
#define HC_SETUP_DTID                   _IOW('k', 1003, clockevent_device_t *)
#define HC_RESTORE_DTID                 _IO('k', 1004)
#define HC_SET_X2APIC_ID                _IO('k', 1005)
#define HC_RESTORE_X2APIC_ID            _IO('k', 1006)
#define HC_DISABLE_INTERCEPT_WRMSR_ICR  _IO('k', 1007)
#define HC_ENABLE_INTERCEPT_WRMSR_ICR   _IO('k', 1008)

/* ioctl */
#define SET_TIMER_EVENT_HANDLER         _IO('k', 2000)
#define RESTORE_TIMER_EVENT_HANDLER     _IO('k', 2001)
#define PRINT_DID                       _IO('k', 2002)
#define SET_CLOCKEVENT_FACTOR           _IOW('k', 2003, clockevent_device_t *)
#define RESTORE_CLOCKEVENT_FACTOR       _IO('k', 2004)

/* ipi */
typedef struct ipi_t {
        int cpu;
        int vector;
} ipi_t;

#define SET_APIC_IPI                    _IO('k', 3000)
#define RESTORE_APIC_IPI                _IO('k', 3001)
#define GET_X2APIC_ID                   _IO('k', 3002)
#define SET_X2APIC_ID                   _IO('k', 3003)
#define SET_X2APIC_ID2                  _IO('k', 3004)
#define RESTORE_X2APIC_ID               _IO('k', 3005)
#define SEND_IPI                        _IOW('k', 3006, ipi_t *)

/* page table */
#define PAGE_WALK_INIT_MM       _IO('k', 4000)

#endif
