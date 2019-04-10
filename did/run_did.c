/*
 * Direct interrupt delivery.
 */
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "did.h"

int main(int argc, char *argv[])
{
        int ret;

        if (argc < 2) {
                fprintf(stderr, "Usage: %s <op> [arg|[arg]]\n", argv[0]);
                fprintf(stderr, "- set_event_handler\n");
                fprintf(stderr, "- restore_event_handler\n");
                fprintf(stderr, "- print_did\n");
                fprintf(stderr, "- set_clockevent_factor\n");
                fprintf(stderr, "- restore_clockevent_factor\n");
                fprintf(stderr, "- set_apic_ipi\n");
                fprintf(stderr, "- restore_apic_ipi\n");
                fprintf(stderr, "- page_talk_init_mm\n");
                fprintf(stderr, "- set_x2apic_id\n");
                fprintf(stderr, "- get_x2apic_id\n");
                fprintf(stderr, "- restore_x2apic_id\n");
                fprintf(stderr, "- send_ipi <cpu> <vector>\n");
                fprintf(stderr, "- hc_set_cpu_exec_vmcs\n");
                fprintf(stderr, "- hc_restore_cpu_exec_vmcs\n");
                fprintf(stderr, "- hc_get_clockevent_mult\n");
                fprintf(stderr, "- hc_get_clockevent_shift\n");
                fprintf(stderr, "- hc_map_pid\n");
                fprintf(stderr, "- hc_unmap_pid\n");
                fprintf(stderr, "- hc_page_walk\n");
                fprintf(stderr, "- hc_set_x2apic_id\n");
                fprintf(stderr, "- hc_restore_x2apic_id\n");
                fprintf(stderr, "- hc_setup_dtid\n");
                fprintf(stderr, "- hc_restore_dtid\n");
                fprintf(stderr, "- hc_disable_intercept_wrmsr_icr\n");
                fprintf(stderr, "- enable_intercept_wrmsr_icr\n");
                return -1;
        }
        char *op = argv[1];

        int fd = open(DEVICE, O_RDWR);
        if (fd < 0) {
                perror("open");
                return -2;
        }

        //unsigned long *args = calloc(argc - 2, sizeof(unsigned long));
        //if (argc > 2) {
        //        char *ptr;
        //        for (int i = 2; i < argc; i++)
        //                args[i - 2] = strtoul(argv[i], &ptr, 10);
        //}

        if (strcmp(op, "set_event_handler") == 0) {
                if (ioctl(fd, SET_TIMER_EVENT_HANDLER) < 0)
                        goto error;
        } else if (strcmp(op, "restore_event_handler") == 0) {
                if (ioctl(fd, RESTORE_TIMER_EVENT_HANDLER) < 0)
                        goto error;
        } else if (strcmp(op, "print_did") == 0) {
                if (ioctl(fd, PRINT_DID) < 0)
                        goto error;
        } else if (strcmp(op, "set_clockevent_factor") == 0) {
                //clockevent_device_t data =
                //        (clockevent_device_t){"lapic", args[0], args[1]};

                if (ioctl(fd, SET_CLOCKEVENT_FACTOR) < 0)
                        goto error;
        } else if (strcmp(op, "restore_clockevent_factor") == 0) {
                if (ioctl(fd, RESTORE_CLOCKEVENT_FACTOR) < 0)
                        goto error;
        } else if (strcmp(op, "set_apic_ipi") == 0) {
                if (ioctl(fd, SET_APIC_IPI) < 0)
                        goto error;
        } else if (strcmp(op, "restore_apic_ipi") == 0) {
                if (ioctl(fd, RESTORE_APIC_IPI) < 0)
                        goto error;
        } else if (strcmp(op, "get_x2apic_id") == 0) {
                if (ioctl(fd, GET_X2APIC_ID) < 0)
                        goto error;
        } else if (strcmp(op, "set_x2apic_id") == 0) {
                if (ioctl(fd, SET_X2APIC_ID) < 0)
                        goto error;
        } else if (strcmp(op, "restore_x2apic_id") == 0) {
                if (ioctl(fd, RESTORE_X2APIC_ID) < 0)
                        goto error;
        } else if (strcmp(op, "set_x2apic_id2") == 0) {
                unsigned long apicid = strtoul(argv[2], NULL, 16);

                if (ioctl(fd, SET_X2APIC_ID2, apicid) < 0)
                        goto error;
        } else if (strcmp(op, "page_walk_init_mm") == 0) {
                if (ioctl(fd, PAGE_WALK_INIT_MM) < 0)
                        goto error;
        } else if (strcmp(op, "send_ipi") == 0) {
                ipi_t ipi;

                ipi.cpu = atoi(argv[2]);
                ipi.vector = strtoul(argv[3], NULL, 16);

                if (ioctl(fd, SEND_IPI, &ipi) < 0)
                        goto error;
        } else if (strcmp(op, "hc_set_cpu_exec_vmcs") == 0) {
                if (ioctl(fd, HC_SET_CPU_EXEC_VMCS) < 0)
                        goto error;
        } else if (strcmp(op, "hc_restore_cpu_exec_vmcs") == 0) {
                if (ioctl(fd, HC_RESTORE_CPU_EXEC_VMCS) < 0)
                        goto error;
        } else if (strcmp(op, "hc_get_clockevent_mult") == 0) {
                if (ioctl(fd, HC_GET_CLOCKEVENT_MULT) < 0)
                        goto error;
        } else if (strcmp(op, "hc_get_clockevent_shift") == 0) {
                if (ioctl(fd, HC_GET_CLOCKEVENT_SHIFT) < 0)
                        goto error;
        } else if (strcmp(op, "hc_map_pid") == 0) {
                if (ioctl(fd, HC_MAP_PID) < 0)
                        goto error;
        } else if (strcmp(op, "hc_unmap_pid") == 0) {
                if (ioctl(fd, HC_UNMAP_PID) < 0)
                        goto error;
        } else if (strcmp(op, "hc_page_walk") == 0) {
                if (ioctl(fd, HC_PAGE_WALK) < 0)
                        goto error;
        } else if (strcmp(op, "hc_set_x2apic_id") == 0) {
                if (ioctl(fd, HC_SET_X2APIC_ID) < 0)
                        goto error;
        } else if (strcmp(op, "hc_restore_x2apic_id") == 0) {
                if (ioctl(fd, HC_RESTORE_X2APIC_ID) < 0)
                        goto error;
        } else if (strcmp(op, "hc_setup_dtid") == 0) {
                //clockevent_device_t data =
                //        (clockevent_device_t){"lapic", args[0], args[1]};

                if (ioctl(fd, HC_SETUP_DTID) < 0)
                        goto error;
        } else if (strcmp(op, "hc_restore_dtid") == 0) {
                if (ioctl(fd, HC_RESTORE_DTID) < 0)
                        goto error;
        } else if (strcmp(op, "hc_disable_intercept_wrmsr_icr") == 0) {
                if (ioctl(fd, HC_DISABLE_INTERCEPT_WRMSR_ICR) < 0)
                        goto error;
        } else if (strcmp(op, "hc_enable_intercept_wrmsr_icr") == 0) {
                if (ioctl(fd, HC_ENABLE_INTERCEPT_WRMSR_ICR) < 0)
                        goto error;
        } else {
                fprintf(stderr, "No such option: %s\n", op);
        }

        //free(args);
        return 0;

error:
        ret = -errno;
        perror("ioctl");
        //free(args);
        return ret;
}
