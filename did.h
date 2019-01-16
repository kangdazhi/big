/*
 * Direct interrupt delivery
 * @author Kevin Cheng
 * @since  01/15/2019
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

/* DTID */
#define KVM_HC_MAP_PID   9
#define KVM_HC_UNMAP_PID 10
#define KVM_HC_PAGE_WALK 11

#define ALLOCATE            _IOW('k', 1, unsigned long)
#define DEALLOCATE          _IO('k', 2)
#define HYPERCALL_MAP_PID   _IO('k', 3)
#define HYPERCALL_UNMAP_PID _IO('k', 4)
#define READ_PIR            _IO('k', 5)
#define WRITE_PIR           _IOW('k', 6, unsigned long)
#define PAGE_WALK           _IO('k', 7)
#define TEST_READ           _IO('k', 8)
#define TEST_WRITE          _IO('k', 9)

/* Clockevent devices */
#define READ_CLOCKEVENT_DEVICES  _IO('k', 10)
#define WRITE_CLOCKEVENT_DEVICES  _IOW('k', 11, clockevent_device_t *)

#define INITIAL_CHAR 'K'

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(const char *))
const char *commands[] = {"allocate", "deallocate", "map", "unmap", "read",
                          "write", "page_walk", "test_read", "test_write",
                          "read_clockevent_device", "write_clockevent_device"};

#endif
