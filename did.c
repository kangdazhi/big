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
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <asm/uaccess.h>
#include <asm/kvm_para.h>
#include <asm/irq_vectors.h>
#include <asm/apic.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

#include "did.h"

static bool dipi = true;
module_param(dipi, bool, 0644);
MODULE_PARM_DESC(dipi, "direct ipi");

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

struct ipi {
        void (*send_IPI)(int cpu, int vector);
        void (*send_IPI_mask)(const struct cpumask *mask, int vector);
        void (*send_IPI_mask_allbutself)(const struct cpumask *mask,
                                         int vector);
        void (*send_IPI_allbutself)(int vector);
        void (*send_IPI_all)(int vector);
        void (*send_IPI_self)(int vector);
};
static struct ipi *ipi;

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
        bool ret;

        dids = kcalloc(ncpus, sizeof(*dids), GFP_ATOMIC);

        if (dids) {
                ret = true;

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
                ret = false;
                pr_alert("Fail to allocate pid array\n");
        }

        ipi = kzalloc(sizeof(*ipi), GFP_ATOMIC);
        if (!ipi) {
                pr_alert("Fail to allocat ipi struct\n");
                ret = false;
        }

        return ret;
}

static void touch_page(void)
{
        if (dids) {
                u32 i;
                const char *data;
                char temp;

                for (i = 0; i < ncpus; i++) {
                        data = (const char *)dids[i].pid;
                        if (data) {
                                temp = data[0];
                                temp = data[PAGE_SIZE - 1];
                        }
                }
        }
}

static void deallocate(void)
{
        if (dids) {
                u32 i;
                void *pid;

                for (i = 0; i < ncpus; i++) {
                        pid = (void *)dids[i].pid;
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

        if (ipi) {
                kfree(ipi);
                ipi = NULL;
        }
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
static void set_posted_interrupt(u32 vector, unsigned long *pid)
{
        __set_bit(vector, pid);
}

static void pi_set_timer_interrupt(unsigned long *pid)
{
        //__set_bit(PI_ON, pid);
        set_posted_interrupt(LOCAL_TIMER_VECTOR, pid);
}

/*
 * Keep the PIR timer-interrupt and ON bit on all the time. We
 * only process the timer interrupt, when it is not the early
 * timer delivery.
 */
static bool bypass_early_timer_interrupt(int cpu, ktime_t next_event)
{
        ktime_t now;
        bool ret;
        bool mapped;

        ret = false;
        mapped = dids && dids[cpu].start;

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
        int cpu;
        struct clock_event_device *evt;

        cpu = smp_processor_id();
        evt = this_cpu_ptr(lapic_events);

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
static void set_timer_event_handler(void)
{
        int cpu;
        struct clock_event_device *evt;

        cpu = smp_processor_id();
        evt = this_cpu_ptr(lapic_events);
        dids[cpu].event_handler = evt->event_handler;

        evt->event_handler = timer_interrupt_handler;
}

static void restore_timer_event_handler(void)
{
        int cpu;
        struct clock_event_device *evt;

        cpu = smp_processor_id();
        evt = this_cpu_ptr(lapic_events);
        evt->event_handler = dids[cpu].event_handler;
}

static bool hc_map_posted_interrupt_descriptor(void)
{
        bool ret;
        unsigned int cpu;
        unsigned long pid;
        int offset;

        cpu = smp_processor_id();
        pid = dids[cpu].pid;
        offset = kvm_hypercall1(KVM_HC_MAP_PID, virt_to_phys((void *)pid));

        if (offset >= 0) {
                dids[cpu].start = (pid & ~0xFFF) | offset;
                pr_info("cpu(%u): mapping pid succeed: 0x%x\n", cpu, offset);
                ret = true;

        } else {
                pr_alert("maping pid fails: %u\t%d\n", cpu, offset);
                ret = false;
        }

        return ret;
}

static bool hc_unmap_posted_interrupt_descriptor(void)
{
        bool ret;
        unsigned int cpu;
        unsigned long pid;
        int res;

        cpu = smp_processor_id();
        pid = dids[cpu].pid;
        res = kvm_hypercall1(KVM_HC_UNMAP_PID, virt_to_phys((void *)pid));

        if (res) {
                dids[cpu].start = 0;
                pr_info("cpu(%u): unmapping pid succeed\n", cpu);
                ret = true;
        } else {
                pr_alert("cpu(%u): unmapping pid fails: %d\n", cpu, res);
                ret = false;
        }

        return ret;
}

static bool hc_page_walk(void)
{
        bool ret;
        unsigned int cpu;
        unsigned long pid;
        int res;

        cpu = smp_processor_id();
        pid = dids[cpu].pid;
        res = kvm_hypercall1(KVM_HC_PAGE_WALK, virt_to_phys((void *)pid));

        if (res) {
                pr_info("cpu(%u): page-walk succeed\n", cpu);
                ret  = true;
        } else {
                pr_alert("cpu(%u): page-walk fails: %d\n", cpu, res);
                ret = false;
        }

        return ret;
}

static unsigned long hc_get_clockevent_factor(const char *query)
{
        unsigned long ret;

        if (strcmp(query, "mult") == 0)
                ret = kvm_hypercall0(KVM_HC_GET_CLOCKEVENT_MULT);
        else if (strcmp(query, "shift") == 0)
                ret = kvm_hypercall0(KVM_HC_GET_CLOCKEVENT_SHIFT);
        else
                ret = 0;

        return ret;
}

static int set_clockevent_factor(void)
{
        int ret;
        u32 mult;
        u32 shift;

        mult = hc_get_clockevent_factor("mult");
        shift = hc_get_clockevent_factor("shift");

        if (mult > 0 && shift > 0) {
                int cpu;
                struct clock_event_device *evt;

                cpu = smp_processor_id();
                evt = this_cpu_ptr(lapic_events);

                dids[cpu].mult = evt->mult;
                dids[cpu].shift = evt->shift;

                evt->mult = mult;
                evt->shift = shift;

                ret = 0;
        } else {
                ret = -EAGAIN;
        }

        return ret;
}

static void restore_clockevent_factor(void)
{
        int cpu;
        struct clock_event_device *evt;

        cpu = smp_processor_id();
        evt = this_cpu_ptr(lapic_events);

        evt->mult = dids[cpu].mult;
        evt->shift = dids[cpu].shift;
}

/*
 * rare write to the read-only memory.
 * https://lwn.net/Articles/724319/
 *
 * Note
 * - x86 only.
 * - CR0 WP control (or bit 16) is 1. The CPU cannot write to
 *   read-only page, when CPL = 0.
 * - NOT for the preemptable kernel.
 * - rare-write mechanism is open for the discussion in the
 *   area of hardening Linux Kernel.
 */
static __always_inline unsigned long x86_rare_write_begin(void)
{
        unsigned long cr0;

        cr0 = read_cr0() ^ X86_CR0_WP;
        BUG_ON(cr0 & X86_CR0_WP);
        write_cr0(cr0);

        return cr0;
}

static __always_inline unsigned long x86_rare_write_end(void)
{
        unsigned long cr0;

        cr0 = read_cr0() ^ X86_CR0_WP;
        BUG_ON(!(cr0 & X86_CR0_WP));
        write_cr0(cr0);

        return cr0;
}

static __always_inline void rare_write_begin(void)
{
        unsigned long cr0;

        local_irq_disable();
        barrier();
        cr0 = x86_rare_write_begin();
        barrier();

        pr_info("%s: 0x%lx\n", __func__, cr0);
}

static __always_inline void rare_write_end(void)
{
        unsigned long cr0;

        barrier();
        cr0 = x86_rare_write_end();
        barrier();
        local_irq_enable();

        pr_info("%s: 0x%lx\n", __func__, cr0);
}

/* IPI */
void print_ipi_dmesg(int cpu, int vector)
{
        int scpu;

        scpu = smp_processor_id();
        trace_printk("%d->%d: 0x%x\n", scpu, cpu, vector);
}

static void did_send_IPI(int cpu, int vector)
{
        if (dipi) {
                unsigned long *pid;

                pid = (unsigned long *)dids[cpu].start;
                set_posted_interrupt(vector, pid);

                ipi->send_IPI(cpu, POSTED_INTR_VECTOR);
        } else {
                ipi->send_IPI(cpu, vector);
        }
        //print_ipi_dmesg(cpu, vector);
}

static void did_send_IPI_mask(const struct cpumask *mask, int vector)
{
        if (dipi) {
                unsigned long cpu;
                unsigned long *pid;

                for_each_cpu(cpu, mask) {
                        pid = (unsigned long *)dids[cpu].start;
                        set_posted_interrupt(vector, pid);
                }

                ipi->send_IPI_mask(mask, POSTED_INTR_VECTOR);
        } else {
                ipi->send_IPI_mask(mask, vector);
        }
}

static void did_send_IPI_mask_allbutself(const struct cpumask *mask, int vector)
{
        if (dipi) {
                unsigned long this_cpu;
                unsigned long query_cpu;
                unsigned long *pid;

                for_each_cpu(query_cpu, mask) {
                        this_cpu = smp_processor_id();
                        if (query_cpu == this_cpu)
                                continue;

                        pid = (unsigned long *)dids[query_cpu].start;
                        set_posted_interrupt(vector, pid);
                }

                ipi->send_IPI_mask(mask, POSTED_INTR_VECTOR);
        } else {
                ipi->send_IPI_mask(mask, vector);
        }
}

static void did_send_IPI_allbutself(int vector)
{
        did_send_IPI_mask_allbutself(cpu_online_mask, vector);
}

static void did_send_IPI_all(int vector)
{
        did_send_IPI_mask(cpu_online_mask, vector);
}

static void did_send_IPI_self(int vector)
{
        if (dipi) {
                unsigned long cpu;
                unsigned long *pid;

                cpu = smp_processor_id();
                pid = (unsigned long *)dids[cpu].start;

                set_posted_interrupt(vector, pid);

                ipi->send_IPI_self(POSTED_INTR_VECTOR);
        } else {
                ipi->send_IPI_self(vector);
        }
}

static void print_apic_ipi(void)
{
        pr_info("send_IPI:                 0x%p\n", apic->send_IPI);
        pr_info("send_IPI_mask:            0x%p\n", apic->send_IPI_mask);
        pr_info("send_IPI_mask_allbutself: 0x%p\n",
                apic->send_IPI_mask_allbutself);
        pr_info("send_IPI_allbutself:      0x%p\n", apic->send_IPI_allbutself);
        pr_info("send_IPI_all:             0x%p\n", apic->send_IPI_all);
        pr_info("send_IPI_self:            0x%p\n", apic->send_IPI_self);
}

static void set_apic_ipi(void)
{
        print_apic_ipi();

        ipi->send_IPI = apic->send_IPI;
        ipi->send_IPI_mask = apic->send_IPI_mask;
        ipi->send_IPI_mask_allbutself = apic->send_IPI_mask_allbutself;
        ipi->send_IPI_allbutself = apic->send_IPI_allbutself;
        ipi->send_IPI_all = apic->send_IPI_all;
        ipi->send_IPI_self = apic->send_IPI_self;

        /* rare-write apic located in .data..ro_after_init */
        rare_write_begin();
        apic->send_IPI = did_send_IPI;
        apic->send_IPI_mask = did_send_IPI_mask;
        apic->send_IPI_mask_allbutself = did_send_IPI_mask_allbutself;
        apic->send_IPI_allbutself = did_send_IPI_allbutself;
        apic->send_IPI_all = did_send_IPI_all;
        apic->send_IPI_self = did_send_IPI_self;
        rare_write_end();

        print_apic_ipi();
}

static void restore_apic_ipi(void)
{
        rare_write_begin();
        apic->send_IPI = ipi->send_IPI;
        apic->send_IPI_mask = ipi->send_IPI_mask;
        apic->send_IPI_mask_allbutself = ipi->send_IPI_mask_allbutself;
        apic->send_IPI_allbutself = ipi->send_IPI_allbutself;
        apic->send_IPI_all = ipi->send_IPI_all;
        apic->send_IPI_self = ipi->send_IPI_self;
        rare_write_end();
}

static void get_x2apic_id(void)
{
        int cpu;
        int apicid;

        cpu = smp_processor_id();
        apicid = per_cpu(x86_cpu_to_apicid, cpu);
        pr_info("phys x2apic id: 0x%x\n", apicid);
}

static void set_x2apic_id(void)
{
        int cpu;
        int apicid;

        rdmsrl(APIC_BASE_MSR + (APIC_ID >> 4), apicid);

        cpu = smp_processor_id();
        per_cpu(x86_cpu_to_apicid, cpu) = apicid;
        pr_info("%s: 0x%x\n", __func__, per_cpu(x86_cpu_to_apicid, cpu));
}

static void restore_x2apic_id(void)
{
        int cpu;

        cpu = smp_processor_id();
        per_cpu(x86_cpu_to_apicid, cpu) = cpu;
        pr_info("%s: 0x%x\n", __func__, per_cpu(x86_cpu_to_apicid, cpu));
}

static void set_x2apic_id2(unsigned long apicid)
{
        int cpu;

        cpu = smp_processor_id();
        per_cpu(x86_cpu_to_apicid, cpu) = apicid;
        pr_info("%s: 0x%x\n", __func__, per_cpu(x86_cpu_to_apicid, cpu));
}

static int send_ipi(unsigned long arg)
{
        int ret;
        int is_bad;
        ipi_t data;

        is_bad = copy_from_user(&data, (ipi_t *)arg, sizeof(ipi_t));

        if (is_bad) {
                ret = -EACCES;
        } else {
                apic->send_IPI(data.cpu, data.vector);
                pr_info("%s: %d->%d: 0x%x\n", __func__, smp_processor_id(),
                                              data.cpu, data.vector);
                ret = 0;
        }

        return ret;
}

static void hc_set_x2apic_id(void)
{
        kvm_hypercall0(KVM_HC_SET_X2APIC_ID);
        set_x2apic_id();
        pr_info("hypercall to set up x2apic id\n");
}

static void hc_restore_x2apic_id(void)
{
        kvm_hypercall0(KVM_HC_RESTORE_X2APIC_ID);
        restore_x2apic_id();
        pr_info("hypercall to set up x2apic id\n");
}

static void hypercall_disable_intercept_wrmsr_icr(void)
{
        kvm_hypercall0(KVM_HC_DISABLE_INTERCEPT_WRMSR_ICR);
        pr_info("hypercall to disable intercept wrmsr icr\n");
}

static void hypercall_enable_intercept_wrmsr_icr(void)
{
        kvm_hypercall0(KVM_HC_ENABLE_INTERCEPT_WRMSR_ICR);
        pr_info("hypercall to enable intercept wrmsr icr\n");
}

/* page table */
static pte_t *page_walk(struct mm_struct *mm, unsigned long addr)
{
        pgd_t *pgd;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;

        pgd = pgd_offset(mm, addr);
        if (pgd_none(*pgd))
                goto out;
        pr_info("PGD: 0x%p\t0x%lx\n", pgd, pgd_val(*pgd));

        pud = pud_offset(pgd, addr);
        if (pud_none(*pud))
                goto out;
        pr_info("PUD: 0x%p\t0x%lx\n", pud, pud_val(*pud));

        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd))
                goto out;
        pr_info("PMD: 0x%p\t0x%lx\n", pmd, pmd_val(*pmd));

        pte = pte_offset_map(pmd, addr);
        if (pte_none(*pte))
                goto out;
        pr_info("PTE: 0x%p\t0x%lx\n", pte, pte_val(*pte));

        return pte;

out:
        return NULL;
}

static int hc_setup_dtid(void)
{
        int ret;
        unsigned long pid;
        unsigned int cpu;
        int offset;

        cpu = smp_processor_id();
        pid = dids[cpu].pid;
        offset = kvm_hypercall1(KVM_HC_SETUP_DTID, virt_to_phys((void *)pid));

        if (offset >= 0) {
                ret = set_clockevent_factor();
                set_timer_event_handler();

                dids[cpu].start = (pid & ~0xFFF) | offset;
                pi_set_timer_interrupt((unsigned long *)dids[cpu].start);
                apic_write(APIC_TMICT, 0x616d);

                pr_info("cpu(%u): setting up dtid succeed: 0x%x\n", cpu, offset);
        } else {
                pr_alert("setting up dtid fails: %u\t%d\n", cpu, offset);
                ret = -EAGAIN;
        }

        return ret;
}

static bool hc_restore_dtid(void)
{
        bool ret;
        unsigned int cpu;
        unsigned long pid;
        int res;

        cpu = smp_processor_id();
        pid = dids[cpu].pid;
        res = kvm_hypercall1(KVM_HC_RESTORE_DTID, virt_to_phys((void *)pid));

        if (res) {
                restore_clockevent_factor();
                restore_timer_event_handler();
                apic_write(APIC_TMICT, 0x616d);

                dids[cpu].start = 0;

                pr_info("cpu(%u): restoring dtid succeed\n", cpu);
                ret = true;
        } else {
                pr_alert("cpu(%u): restoring dtid fails: %d\n", cpu, res);
                ret = false;
        }

        return ret;
}

/* vmcs */
static void hc_set_cpu_exec_vmcs(void)
{
        kvm_hypercall0(KVM_HC_SET_CPU_EXEC_VMCS);
        pr_info("hypercall to set the primary cpu execution vmcs\n");
}

static void hc_restore_cpu_exec_vmcs(void)
{
        kvm_hypercall0(KVM_HC_RESTORE_CPU_EXEC_VMCS);
        pr_info("hypercall to restore the primary cpu execution vmcs\n");
}

static long my_ioctl(struct file *fobj, unsigned int cmd, unsigned long arg)
{
        long ret = 0;

        switch (cmd) {
        case SET_TIMER_EVENT_HANDLER:
                set_timer_event_handler();
                break;
        case RESTORE_TIMER_EVENT_HANDLER:
                restore_timer_event_handler();
                break;
        case PRINT_DID:
                print_did();
                break;
        case SET_CLOCKEVENT_FACTOR:
                ret = set_clockevent_factor();
                break;
        case RESTORE_CLOCKEVENT_FACTOR:
                restore_clockevent_factor();
                break;
        case SET_APIC_IPI:
                set_apic_ipi();
                break;
        case RESTORE_APIC_IPI:
                restore_apic_ipi();
                break;
        case GET_X2APIC_ID:
                get_x2apic_id();
                break;
        case SET_X2APIC_ID:
                set_x2apic_id();
                break;
        case RESTORE_X2APIC_ID:
                restore_x2apic_id();
                break;
        case SET_X2APIC_ID2:
                set_x2apic_id2(arg);
                break;
        case SEND_IPI:
                send_ipi(arg);
                break;
        case HC_SET_CPU_EXEC_VMCS:
                hc_set_cpu_exec_vmcs();
                break;
        case HC_RESTORE_CPU_EXEC_VMCS:
                hc_restore_cpu_exec_vmcs();
                break;
        case HC_GET_CLOCKEVENT_MULT:
                hc_get_clockevent_factor("mult");
                break;
        case HC_GET_CLOCKEVENT_SHIFT:
                hc_get_clockevent_factor("shift");
                break;
        case HC_MAP_PID:
                if (!hc_map_posted_interrupt_descriptor())
                        ret = -EAGAIN;
                break;
        case HC_UNMAP_PID:
                if (!hc_unmap_posted_interrupt_descriptor())
                        ret = -EAGAIN;
                break;
        case HC_PAGE_WALK:
                if (!hc_page_walk())
                        ret = -EAGAIN;
                break;
        case HC_SETUP_DTID:
                ret = hc_setup_dtid();
                break;
        case HC_RESTORE_DTID:
                if (!hc_restore_dtid())
                        ret = -EAGAIN;
                break;
        case HC_SET_X2APIC_ID:
                hc_set_x2apic_id();
                break;
        case HC_RESTORE_X2APIC_ID:
                hc_restore_x2apic_id();
                break;
        case HC_DISABLE_INTERCEPT_WRMSR_ICR:
                hypercall_disable_intercept_wrmsr_icr();
                break;
        case HC_ENABLE_INTERCEPT_WRMSR_ICR:
                hypercall_enable_intercept_wrmsr_icr();
                break;
        case PAGE_WALK_INIT_MM: {
                struct mm_struct *mm =
                        (struct mm_struct *) kallsyms_lookup_name("init_mm");
                unsigned long apic_driver =
                        kallsyms_lookup_name("apic_x2apic_phys");

                pr_info("apic_driver: 0x%lx\n", __pa(apic_driver));
                pr_info("apic_driver: 0x%lx\n", apic_driver);
                page_walk(mm, apic_driver);

                pr_info("apic: 0x%lx\n", __pa(&apic));
                pr_info("apic: 0x%p\n", &apic);
                page_walk(mm, (unsigned long)&apic);

                break;
        }
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
                unsigned long clock_event =
                        kallsyms_lookup_name("lapic_events");
                lapic_events = (struct clock_event_device *)clock_event;
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
