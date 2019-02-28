/*
 * Direct interrupt delivery
 */
#ifndef _DID_H
#define _DID_H

#define DEVICE_NAME "did"
#define DEVICE "/dev/" DEVICE_NAME

/* Clockevent devices */
typedef struct clockevent_device_t {
        char name[10];
        unsigned int mult;
        unsigned int shift;
} clockevent_device_t;

/* hypercalls */
#define KVM_HC_SETUP_DID        100
#define KVM_HC_RESTORE_DID      101
#define KVM_HC_MAP_PID          200
#define KVM_HC_UNMAP_PID        201
#define KVM_HC_PAGE_WALK        202

/* ioctls to hypercalls */
#define MAP_PID         _IO('k', 1000)
#define UNMAP_PID       _IO('k', 1001)
#define PAGE_WALK       _IO('k', 1002)
#define SETUP_DID       _IOW('k', 1003, clockevent_device_t *)
#define RESTORE_DID     _IO('k', 1004)

/* ioctls */
#define SET_EVENT_HANDLER               _IO('k', 2000)
#define RESTORE_EVENT_HANDLER           _IO('k', 2001)
#define PRINT_DID                       _IO('k', 2002)
#define SET_CLOCKEVENT_FACTOR           _IOW('k', 2003, clockevent_device_t *)
#define RESTORE_CLOCKEVENT_FACTOR       _IO('k', 2004)

#endif
