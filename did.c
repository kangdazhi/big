/*
 * Direct interrupt delivery.
 * @author Kevin Cheng
 * @since  01/15/2019
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <asm/kvm_para.h>
#include <linux/ktime.h>
#include <linux/kallsyms.h>
#include <linux/list.h>
#include <linux/clockchips.h>

#include "did.h"

/* Clockevent devices */
typedef struct list_head list_head;
static list_head *clockevent_devices;
static struct clock_event_device *device;
static struct clock_event_device *temp;

/* DTID */
struct osnet_dtid_pi_desc {
    unsigned long pid;      /* PID vaddr */
    unsigned long start;    /* PID start addr */
    ktime_t entry_time;     /* entry time to the APID timer interrupt handler */
};
extern struct osnet_dtid_pi_desc *osnet_pids;
static unsigned int ncpus = 0;

static int my_open(struct inode *iobj, struct file *fobj)
{
    return 0;
}

static int my_release(struct inode *iobj, struct file *fobj)
{
    return 0;
}

/* Clockevent devices */
void read_clockevent_devices(void)
{
    clockevent_devices =
        (list_head *) kallsyms_lookup_name("clockevent_devices");
    list_for_each_entry_safe(device, temp, clockevent_devices, list) {
        pr_info("%s\t%d\t%d\n", device->name, device->mult, device->shift);
    }
}

int write_clockevent_devices(unsigned long arg)
{
    clockevent_device_t data;
    int is_bad = copy_from_user(&data, (clockevent_device_t *) arg,
                                sizeof(clockevent_device_t));
    if (is_bad) return -EACCES;
    clockevent_devices =
        (list_head *) kallsyms_lookup_name("clockevent_devices");
    list_for_each_entry_safe(device, temp, clockevent_devices, list) {
        if (strcmp(device->name, data.name) == 0) {
            device->mult = data.mult;
            device->shift = data.shift;
            pr_info("%s\t%d\t%d\n", device->name, device->mult, device->shift);
        }
    }
    return 0;
}

/* DTID */
static bool allocate(void)
{
    int i;

    if (ncpus == 0)
        return false;

    osnet_pids = kcalloc(ncpus, sizeof(*osnet_pids), GFP_ATOMIC);
    if (osnet_pids) {
        pr_info("pid array vaddr: 0x%p\n", osnet_pids);
        pr_info("pid array paddr: 0x%llx\n", virt_to_phys(osnet_pids));

        for (i = 0; i < ncpus; i++) {
            void *page = kmalloc(PAGE_SIZE, GFP_ATOMIC);
            if (page) {
                osnet_pids[i].pid = (unsigned long) page;
                pr_info("pid page vaddr : 0x%lx\n", osnet_pids[i].pid);
                pr_info("pid page paddr : 0x%llx\n",
                        virt_to_phys((void *)osnet_pids[i].pid));
            } else {
                pr_alert("Fail to kmalloc a page\n");
                return false;
            }
        }
    } else {
        pr_alert("Fail to allocate pid array\n");
        return false;
    }

    return true;
}

static void deallocate(void)
{
    int i;

    if (osnet_pids) {
        for (i = 0; i < ncpus; i++) {
            void *pid = (void *)osnet_pids[i].pid;
            if (pid) {
                kfree(pid);
                pr_info("pid page is freed : 0x%p\n", pid);
            }
            else {
                pr_alert("pid page is NULL\n");
            }

            osnet_pids[i].pid = 0;
            osnet_pids[i].start = 0;
            osnet_pids[i].entry_time = 0;
        }
        kfree(osnet_pids);
        pr_info("pid array is freed: 0x%p\n", osnet_pids);
        osnet_pids = NULL;
    }
}

static bool map_posted_interrupt_descriptor(void)
{
    int offset;
    unsigned int cpu;
    unsigned long pid;

    cpu = smp_processor_id();
    pid = osnet_pids[cpu].pid;

    offset = (int) kvm_hypercall1(KVM_HC_MAP_PID, virt_to_phys((void *)pid));
    if (offset < 0) {
        pr_alert("kvm_hypercall1 fails: %u\t%d\n", cpu, offset);
        return false;
    }
    else {
        unsigned long start = (pid & ~0xFFF) | offset;
        osnet_pids[cpu].start = start;

        pr_info("cpu(%u): kvm_hypercall1 succeed: 0x%x\n", cpu, offset);
        pr_info("cpu(%u): start: 0x%lx\n", cpu, osnet_pids[cpu].start);
    }

    return true;
}

static bool unmap_posted_interrupt_descriptor(void)
{
    int ret;
    unsigned int cpu;
    unsigned long pid;

    cpu = smp_processor_id();
    pid = osnet_pids[cpu].pid;

    ret = (int) kvm_hypercall1(KVM_HC_UNMAP_PID, virt_to_phys((void *)pid));
    if (ret) {
        osnet_pids[cpu].start = 0;
        pr_info("cpu(%u): kvm_hypercall1 succeed\n", cpu);
    }
    else {
        pr_alert("cpu(%u): kvm_hypercall1 fails: %d\n", cpu, ret);
        return false;
    }

    return true;
}

static void test_read_posted_interrupt_request(void)
{
    int k;
    unsigned int cpu;

    cpu = smp_processor_id();
    for (k = 0; k < 600000; k++) {
        int i;
        char temp;
        const char *data;

        data = (const char *)osnet_pids[cpu].start;
        for (i = 0; i < PAGE_SIZE; i++)
            temp = data[i];
        usleep_range(100, 120);
    }
}

static void test_write_posted_interrupt_request(void)
{
    int k;
    unsigned int cpu;

    cpu = smp_processor_id();
    for (k = 0; k < 600000; k++) {
        int i;
        char *data = (char *)osnet_pids[cpu].start;

        /* PID is in 0 - 63 Bytes */
        for (i = 64; i < PAGE_SIZE; i++)
            data[i] = '\0';
        usleep_range(100, 120);
    }
}

static void read_posted_interrupt_request(void)
{
    int i;
    unsigned int *data;
    unsigned int cpu;

    cpu = smp_processor_id();
    data = (unsigned int*)osnet_pids[cpu].start;
    for (i = 7; i >=0 ; i--)
        pr_info("cpu(%u): %d\t0x%x\n", cpu, i, data[i]);
}

static void write_posted_interrupt_request(unsigned long vector)
{
    unsigned int cpu;

    cpu = smp_processor_id();
    if (test_and_set_bit(vector, (unsigned long *)osnet_pids[cpu].start))
        pr_info("cpu(%u): 0x%lx is set\n", cpu, vector);
    else
        pr_info("cpu(%u): Set 0x%lx\n", cpu, vector);
}

static bool page_walk(void)
{
    int ret;
    unsigned int cpu;
    unsigned long pid;

    cpu = smp_processor_id();
    pid = osnet_pids[cpu].pid;
    ret = (int) kvm_hypercall1(KVM_HC_PAGE_WALK, virt_to_phys((void *)pid));
    if (!ret) {
        pr_alert("cpu(%u): kvm_hypercall1 fails: %d\n", cpu, ret);
        return false;
    }
    return true;
}

static long my_ioctl(struct file *fobj, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case ALLOCATE:
            ncpus = arg;
            if (!allocate())
                return -1;
            break;
        case DEALLOCATE:
            deallocate();
            break;
        case HYPERCALL_MAP_PID:
            if(!map_posted_interrupt_descriptor())
                return -1;
            break;
        case HYPERCALL_UNMAP_PID:
            if(!unmap_posted_interrupt_descriptor())
                return -1;
            break;
        case READ_PIR:
            read_posted_interrupt_request();
            break;
        case WRITE_PIR:
            write_posted_interrupt_request(arg);
            break;
        case PAGE_WALK:
            if(!page_walk())
                return -1;
            break;
        case TEST_READ:
            test_read_posted_interrupt_request();
            break;
        case TEST_WRITE:
            test_write_posted_interrupt_request();
            break;
        case READ_CLOCKEVENT_DEVICES:
            read_clockevent_devices();
            break;
        case WRITE_CLOCKEVENT_DEVICES:
            write_clockevent_devices(arg);
            break;
        default:
            pr_alert("No such an ioctl option.\n");
            return -EINVAL;
    }

    return 0;
}

static struct file_operations misc_device_operations = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
    .unlocked_ioctl = my_ioctl
};

static struct miscdevice misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &misc_device_operations
};

static int __init my_init(void)
{
    int is_error = misc_register(&misc_device);
    if (is_error < 0) {
        pr_alert("Fail to register %s\n", DEVICE);
        return is_error;
    }
    else {
        pr_info("Register %s\n", DEVICE);
    }

    return 0;
}

static void __exit my_exit(void)
{
    deallocate();
    misc_deregister(&misc_device);
    pr_info("Bye %s\n", DEVICE);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Cheng");
MODULE_DESCRIPTION("Direct interrupt delivery.");
