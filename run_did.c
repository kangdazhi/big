/*
 * Direct interrupt delivery.
 * @author Kevin Cheng
 * @since  01/15/2019
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

unsigned int select_cmd(const char *cmd)
{
    for (int i = 0; i < ARRAY_LENGTH(commands); i++)
        if (strcasecmp(cmd, commands[i]) == 0)
            return i+1;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <CMD> [ARG|[ARG]]\n", argv[0]);
        fprintf(stderr, "- ALLOCATE <NCPUS>\n");
        fprintf(stderr, "- DEALLOCATE\n");
        fprintf(stderr, "- MAP   PID\n");
        fprintf(stderr, "- UNMAP PID\n");
        fprintf(stderr, "- READ  PIR\n");
        fprintf(stderr, "- WRITE <VECTOR>\n");
        fprintf(stderr, "- PAGE  WALK\n");
        fprintf(stderr, "- TEST  READ\n");
        fprintf(stderr, "- TEST  WRITE\n");
        fprintf(stderr, "- READ CLOCKEVENT DEVICE\n");
        fprintf(stderr, "- WRITE CLOCKEVENT DEVICE\n");
        return -1;
    }
    unsigned int opt = select_cmd(argv[1]);


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

    switch (opt) {
        clockevent_device_t data;

        case 1:
            if (ioctl(fd, ALLOCATE, args[0]) < 0)
                goto error;
            break;
        case 2:
            if (ioctl(fd, DEALLOCATE) < 0)
                goto error;
            break;
        case 3:
            if (ioctl(fd, HYPERCALL_MAP_PID) < 0)
                goto error;
            break;
        case 4:
            if (ioctl(fd, HYPERCALL_UNMAP_PID) < 0)
                goto error;
            break;
        case 5:
            if (ioctl(fd, READ_PIR) < 0)
                goto error;
            break;
        case 6:
            if (ioctl(fd, WRITE_PIR, args[0]) < 0)
                goto error;
            break;
        case 7:
            if (ioctl(fd, PAGE_WALK) < 0)
                goto error;
            break;
        case 8:
            if (ioctl(fd, TEST_READ) < 0)
                goto error;
            break;
        case 9:
            if (ioctl(fd, TEST_WRITE) < 0)
                goto error;
            break;
        case 10:
            if (ioctl(fd, READ_CLOCKEVENT_DEVICES) < 0)
                perror("ioctl");
            break;
        case 11:
            data = (clockevent_device_t){"lapic", args[0], args[1]};
            if (ioctl(fd, WRITE_CLOCKEVENT_DEVICES, data) < 0)
                perror("ioctl");
            break;
        default:
            fprintf(stderr, "No such option: %d\n", opt);
    }

    free(args);
    return 0;

error:
    perror("ioctl");
    free(args);
    return -3;
}
