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

//unsigned int select_cmd(const char *cmd)
//{
//        for (int i = 0; i < ARRAY_LENGTH(commands); i++)
//                if (strcasecmp(cmd, commands[i]) == 0)
//                        return i+1;
//        return 0;
//}

int main(int argc, char *argv[])
{
        if (argc < 2) {
                fprintf(stderr, "Usage: %s <op> [arg|[arg]]\n", argv[0]);
                fprintf(stderr, "- set_event_handler\n");
                fprintf(stderr, "- restore_event_handler\n");
                fprintf(stderr, "- print_did\n");
                fprintf(stderr, "- map_pid\n");
                fprintf(stderr, "- unmap_pid\n");
                fprintf(stderr, "- page_walk\n");
#if 0
                fprintf(stderr, "- read\n");
                fprintf(stderr, "- write <vector>\n");
                fprintf(stderr, "- test_read\n");
                fprintf(stderr, "- test_write\n");
                fprintf(stderr, "- read_clockevent_device\n");
                fprintf(stderr, "- write_clockevent_device\n");
#endif
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
                if (ioctl(fd, SET_EVENT_HANDLER) < 0) goto error;
        } else if (strcmp(op, "restore_event_handler") == 0) {
                if (ioctl(fd, RESTORE_EVENT_HANDLER) < 0) goto error;
        } else if (strcmp(op, "print_did") == 0) {
                if (ioctl(fd, PRINT_DID) < 0) goto error;
        } else if (strcmp(op, "map_pid") == 0) {
                if (ioctl(fd, HYPERCALL_MAP_PID) < 0) goto error;
        } else if (strcmp(op, "unmap_pid") == 0) {
                if (ioctl(fd, HYPERCALL_UNMAP_PID) < 0) goto error;
        } else if (strcmp(op, "page_walk") == 0) {
                if (ioctl(fd, HYPERCALL_PAGE_WALK) < 0) goto error;
        } else {
                fprintf(stderr, "No such option: %s\n", op);
        }


#if 0
        clockevent_device_t data;
        switch (opt) {
        case 1:
                break;
        case 2:
                break;
        case 3:
                if (ioctl(fd, HYPERCALL_MAP_PID) < 0) goto error;
                break;
        case 4:
                if (ioctl(fd, HYPERCALL_UNMAP_PID) < 0) goto error;
                break;
        case 5:
                if (ioctl(fd, READ_PIR) < 0) goto error;
                break;
        case 6:
                if (ioctl(fd, WRITE_PIR, args[0]) < 0) goto error;
                break;
        case 7:
                if (ioctl(fd, PAGE_WALK) < 0) goto error;
                break;
        case 8:
                if (ioctl(fd, TEST_READ) < 0) goto error;
                break;
        case 9:
                if (ioctl(fd, TEST_WRITE) < 0) goto error;
                break;
        case 10:
                if (ioctl(fd, READ_CLOCKEVENT_DEVICES) < 0) perror("ioctl");
                break;
        case 11:
                data = (clockevent_device_t){"lapic", args[0], args[1]};
                if (ioctl(fd, WRITE_CLOCKEVENT_DEVICES, &data) < 0)
                        perror("ioctl");
                break;
        default:
                fprintf(stderr, "No such option: %d\n", opt);
        }

#endif
        free(args);
        return 0;

error:
        perror("ioctl");
        free(args);
        return -3;
}
