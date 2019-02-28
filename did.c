/*
 * Direct interrupt delivery.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/kallsyms.h>
#include <linux/list.h>
#include <linux/clockchips.h>
#include <linux/cpumask.h>
#include <asm/uaccess.h>
#include <asm/kvm_para.h>
#include <asm/irq_vectors.h>
#include <asm/apic.h>

#include "did.h"

static u32 ncpus;
static struct clock_event_device *lapic_events;
struct did {
        /* posted-intr descriptor */
        unsigned long pid;      /* pid page  vaddr */
        unsigned long start;    /* pid start vaddr */

        /* clockevent device */
        u32 mult;               /* old clock multiplication factor */
        u32 shift;              /* old clock shift factor */
                                /* old LAPIC handler */
        void (*event_handler)(struct clock_event_device *);
};
static struct did *dids;

static int my_open(struct inode *iobj, struct file *fobj)
{
        return 0;
}

static int my_release(struct inode *iobj, struct file *fobj)
{
        return 0;
}

static void print_did(void)
{
        u32 i;

        pr_info("-------------------------");
        pr_info("pid array vaddr: 0x%p\n", dids);
        pr_info("pid array paddr: 0x%llx\n", virt_to_phys(dids));

        for (i = 0; i < ncpus; i++) {
                unsigned long long paddr = virt_to_phys((void *)dids[i].pid);

                pr_info("*************************");
                pr_info("cpu            : %u", i);
                pr_info("pid page vaddr : 0x%lx\n", dids[i].pid);
                pr_info("pid page paddr : 0x%llx\n", paddr);
                pr_info("start          : 0x%lx\n", dids[i].start);
                pr_info("mult           : %u\n", dids[i].mult);
                pr_info("shift          : %u\n", dids[i].shift);
                pr_info("event_handler  : 0x%p\n", dids[i].event_handler);
        }
}

static bool allocate(void)
{
        u32 i;
        bool ret = true;
        dids = kcalloc(ncpus, sizeof(*dids), GFP_ATOMIC);

        if (dids) {
                for (i = 0; i < ncpus; i++) {
                        void *page = kmalloc(PAGE_SIZE, GFP_ATOMIC);
                        if (page) {
                                dids[i].pid = (unsigned long)page;
                        } else {
                                pr_alert("Fail to kmalloc a page\n");
                                ret = false;
                        }
                }
        } else {
                pr_alert("Fail to allocate pid array\n");
                ret = false;
        }

        return ret;
}

static void touch_page(void)
{
        u32 i;

        if (!dids)
                return;

        for (i = 0; i < ncpus; i++) {
                const char *data = (const char *)dids[i].pid;
                if (data) {
                        char temp;
                        temp = data[0];
                        temp = data[PAGE_SIZE - 1];
                }
        }
}

static void deallocate(void)
{
        u32 i;

        if (!dids)
                return;

        for (i = 0; i < ncpus; i++) {
                void *pid = (void *)dids[i].pid;
                kfree(pid);
                pr_info("pid page is freed : 0x%p\n", pid);

                dids[i].pid = 0;
                dids[i].start = 0;
                dids[i].mult = 0;
                dids[i].shift = 0;
                dids[i].event_handler = NULL;
        }

        kfree(dids);
        pr_info("pid array is freed: 0x%p\n", dids);
        dids = NULL;
}

/*
 * test_and_set_bit uses the LOCK prefix. It achieve the
 * atomicity by locking the cache line to the shared memory.
 * This ensures the processor has the exclusive ownership of
 * shared memory for the duration of operation. However, it
 * also prevents the VT-d chipset to modify the shared memory
 * containing, PID. Since we are only setting the PIR
 * timer-interrupt bit without touching other PIR bits and the
 * guest is in the control of LAPIC timer chip, it is safe to
 * use the non-atomic set operation for the PIR
 * timer-interrupt bit.
 */
//#define PI_ON 0x100   /* ON bit is at 256 */
static void pi_set_timer_interrupt(unsigned long *addr)
{
        __set_bit(LOCAL_TIMER_VECTOR, addr);
        //__set_bit(PI_ON, addr);
}

/*
 * Keep the PIR timer-interrupt and ON bit on all the time. We
 * only process the timer interrupt, when it is not the early
 * timer delivery.
 */
static bool bypass_early_timer_interrupt(int cpu, ktime_t next_event)
{
        ktime_t now;
        bool ret = false;
        bool mapped = dids && dids[cpu].start;

        if (mapped) {
                pi_set_timer_interrupt((unsigned long *)dids[cpu].start);

                now = ktime_get();
                if (now < next_event)
                        ret = true;
        }

        return ret;
}

static void timer_interrupt_handler(struct clock_event_device *dev)
{
        void (*event_handler)(struct clock_event_device *);
        int cpu = smp_processor_id();
        struct clock_event_device *evt = this_cpu_ptr(lapic_events);

        if (bypass_early_timer_interrupt(cpu, evt->next_event))
                return;

        event_handler = dids[cpu].event_handler;
        event_handler(evt);
}

/*
 * lapic_events is a per_cpu variable storing the clockevent
 * handler. The usespace program have to call the module on
 * every online CPUs.
 */
static void set_percpu_event_handler(void)
{
        int cpu = smp_processor_id();
        struct clock_event_device *evt = this_cpu_ptr(lapic_events);

        dids[cpu].event_handler = evt->event_handler;
        evt->event_handler = timer_interrupt_handler;
}

static void restore_percpu_event_handler(void)
{
        int cpu = smp_processor_id();
        struct clock_event_device *evt = this_cpu_ptr(lapic_events);

        evt->event_handler = dids[cpu].event_handler;
}

static bool map_percpu_posted_interrupt_descriptor(void)
{
        bool ret = true;
        unsigned int cpu = smp_processor_id();
        unsigned long pid = dids[cpu].pid;
        int offset = kvm_hypercall1(KVM_HC_MAP_PID, virt_to_phys((void *)pid));

        if (offset >= 0) {
                dids[cpu].start = (pid & ~0xFFF) | offset;
                pr_info("cpu(%u): mapping pid succeed: 0x%x\n", cpu, offset);
        } else {
                pr_alert("maping pid fails: %u\t%d\n", cpu, offset);
                ret = false;
        }

        return ret;
}

static bool unmap_percpu_posted_interrupt_descriptor(void)
{
        bool ret = true;
        unsigned int cpu = smp_processor_id();
        unsigned long pid = dids[cpu].pid;
        int res = kvm_hypercall1(KVM_HC_UNMAP_PID, virt_to_phys((void *)pid));

        if (res) {
                dids[cpu].start = 0;
                pr_info("cpu(%u): unmapping pid succeed\n", cpu);
        } else {
                pr_alert("cpu(%u): unmapping pid fails: %d\n", cpu, res);
                ret = false;
        }

        return ret;
}

static bool percpu_page_walk(void)
{
        bool ret = true;
        unsigned int cpu = smp_processor_id();
        unsigned long pid = dids[cpu].pid;
        int res = kvm_hypercall1(KVM_HC_PAGE_WALK, virt_to_phys((void *)pid));

        if (res) {
                pr_info("cpu(%u): page-walk succeed\n", cpu);
        } else {
                pr_alert("cpu(%u): page-walk fails: %d\n", cpu, res);
                ret = false;
        }

        return ret;
}

static int set_percpu_clockevent_factor(unsigned long arg)
{
        int ret = 0;
        clockevent_device_t data;
        int cpu = smp_processor_id();
        struct clock_event_device *evt = this_cpu_ptr(lapic_events);
        int is_bad = copy_from_user(&data, (clockevent_device_t *)arg,
                                    sizeof(clockevent_device_t));
        if (is_bad)
                ret = -EACCES;

        dids[cpu].mult = evt->mult;
        dids[cpu].shift = evt->shift;

        evt->mult = data.mult;
        evt->shift = data.shift;

        return ret;
}

static void restore_percpu_clockevent_factor(void)
{
        int cpu = smp_processor_id();
        struct clock_event_device *evt = this_cpu_ptr(lapic_events);

        evt->mult = dids[cpu].mult;
        evt->shift = dids[cpu].shift;
}

static int setup_percpu_did(unsigned long user_arg)
{
        int ret = 0;
        unsigned int cpu = smp_processor_id();
        unsigned long pid = dids[cpu].pid;
        int offset;

        ret = set_percpu_clockevent_factor(user_arg);

        offset = kvm_hypercall1(KVM_HC_SETUP_DID, virt_to_phys((void *)pid));

        if (offset >= 0) {
                dids[cpu].start = (pid & ~0xFFF) | offset;
                pr_info("cpu(%u): setting up did succeed: 0x%x\n", cpu, offset);

                set_percpu_event_handler();

                pi_set_timer_interrupt((unsigned long *)dids[cpu].start);
                apic_write(APIC_TMICT, 0x616d);
        } else {
                pr_alert("setting up did fails: %u\t%d\n", cpu, offset);
                ret = -EPERM;
        }

        return ret;
}

static bool restore_percpu_did(void)
{
        bool ret = true;
        unsigned int cpu = smp_processor_id();
        unsigned long pid = dids[cpu].pid;
        int res;

        restore_percpu_event_handler();

        res = kvm_hypercall1(KVM_HC_RESTORE_DID, virt_to_phys((void *)pid));

        if (res) {
                dids[cpu].start = 0;
                pr_info("cpu(%u): restoring did succeed\n", cpu);
        } else {
                pr_alert("cpu(%u): restoring did fails: %d\n", cpu, res);
                ret = false;
        }

        restore_percpu_clockevent_factor();
        apic_write(APIC_TMICT, 0x616d);

        return ret;
}

static long my_ioctl(struct file *fobj, unsigned int cmd, unsigned long arg)
{
        long ret = 0;

        switch (cmd) {
        case SET_EVENT_HANDLER:
                set_percpu_event_handler();
                break;
        case RESTORE_EVENT_HANDLER:
                restore_percpu_event_handler();
                break;
        case PRINT_DID:
                print_did();
                break;
        case MAP_PID:
                if (!map_percpu_posted_interrupt_descriptor())
                        ret = -EPERM;
                break;
        case UNMAP_PID:
                if (!unmap_percpu_posted_interrupt_descriptor())
                        ret = -EPERM;
                break;
        case PAGE_WALK:
                if (!percpu_page_walk())
                        ret = -EPERM;
                break;
        case SET_CLOCKEVENT_FACTOR:
                ret = set_percpu_clockevent_factor(arg);
                break;
        case RESTORE_CLOCKEVENT_FACTOR:
                restore_percpu_clockevent_factor();
                break;
        case SETUP_DID:
                ret = setup_percpu_did(arg);
                break;
        case RESTORE_DID:
                if (!restore_percpu_did())
                        ret = -EPERM;
                break;
        default:
                pr_alert("No such an ioctl option.\n");
                ret = -EINVAL;
        }

        return ret;
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
        int ret = misc_register(&misc_device);

        if (ret >= 0) {
                unsigned long per_cpu_clock_event =
                        kallsyms_lookup_name("lapic_events");
                lapic_events = (struct clock_event_device *) per_cpu_clock_event;
                ncpus = num_online_cpus();

                allocate();
                touch_page();

                pr_info("Register %s\n", DEVICE);

        } else {
                pr_alert("Fail to register %s\n", DEVICE);
        }

        return ret;
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
