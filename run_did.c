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
        if (argc < 2) {
                fprintf(stderr, "Usage: %s <percpu op> [arg|[arg]]\n", argv[0]);
                fprintf(stderr, "- set_event_handler\n");
                fprintf(stderr, "- restore_event_handler\n");
                fprintf(stderr, "- print_did\n");
                fprintf(stderr, "- map_pid\n");
                fprintf(stderr, "- unmap_pid\n");
                fprintf(stderr, "- page_walk\n");
                fprintf(stderr, "- set_clockevent_factor\n");
                fprintf(stderr, "- restore_clockevent_factor\n");
                fprintf(stderr, "- setup_did\n");
                fprintf(stderr, "- restore_did\n");
                return -1;
        }
        char *op = argv[1];

        int fd = open(DEVICE, O_RDWR);
        if (fd < 0) {
                perror("open");
                return -2;
        }

        unsigned long *args = calloc(argc - 2, sizeof(unsigned long));
        if (argc > 2) {
                char *ptr;
                for (int i = 2; i < argc; i++)
                        args[i - 2] = strtoul(argv[i], &ptr, 10);
        }

        if (strcmp(op, "set_event_handler") == 0) {
                if (ioctl(fd, SET_EVENT_HANDLER) < 0)
                        goto error;
        } else if (strcmp(op, "restore_event_handler") == 0) {
                if (ioctl(fd, RESTORE_EVENT_HANDLER) < 0)
                        goto error;
        } else if (strcmp(op, "print_did") == 0) {
                if (ioctl(fd, PRINT_DID) < 0)
                        goto error;
        } else if (strcmp(op, "map_pid") == 0) {
                if (ioctl(fd, MAP_PID) < 0)
                        goto error;
        } else if (strcmp(op, "unmap_pid") == 0) {
                if (ioctl(fd, UNMAP_PID) < 0)
                        goto error;
        } else if (strcmp(op, "page_walk") == 0) {
                if (ioctl(fd, PAGE_WALK) < 0)
                        goto error;
        } else if (strcmp(op, "set_clockevent_factor") == 0) {
                clockevent_device_t data =
                        (clockevent_device_t){"lapic", args[0], args[1]};

                if (ioctl(fd, SET_CLOCKEVENT_FACTOR, &data) < 0)
                        goto error;
        } else if (strcmp(op, "restore_clockevent_factor") == 0) {
                if (ioctl(fd, RESTORE_CLOCKEVENT_FACTOR) < 0)
                        goto error;
        } else if (strcmp(op, "setup_did") == 0) {
                clockevent_device_t data =
                        (clockevent_device_t){"lapic", args[0], args[1]};

                if (ioctl(fd, SETUP_DID, &data) < 0)
                        goto error;
        } else if (strcmp(op, "restore_did") == 0) {
                if (ioctl(fd, RESTORE_DID) < 0)
                        goto error;
        } else {
                fprintf(stderr, "No such option: %s\n", op);
        }

        free(args);
        return 0;

error:
        perror("ioctl");
        free(args);
        return -3;
}
