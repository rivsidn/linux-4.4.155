/*
 * Common interrupt code for 32 and 64 bit
 */
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/ftrace.h>
#include <linux/delay.h>
#include <linux/export.h>

#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/irq.h>
#include <asm/idle.h>
#include <asm/mce.h>
#include <asm/hw_irq.h>
#include <asm/desc.h>

#define CREATE_TRACE_POINTS
#include <asm/trace/irq_vectors.h>

DEFINE_PER_CPU_SHARED_ALIGNED(irq_cpustat_t, irq_stat);
EXPORT_PER_CPU_SYMBOL(irq_stat);

DEFINE_PER_CPU(struct pt_regs *, irq_regs);
EXPORT_PER_CPU_SYMBOL(irq_regs);

atomic_t irq_err_count;

/* Function pointer for generic interrupt vector handling */
void (*x86_platform_ipi_callback)(void) = NULL;

/*
 * 'what should we do if we get a hw irq event on an illegal vector'.
 * each architecture has to answer this themselves.
 * '如果我们在非法的中断向量上获取到了一个硬件中断该怎么处理？'
 * 不同的架构必须要自己处理这类事件。
 */
void ack_bad_irq(unsigned int irq)
{
	if (printk_ratelimit())
		pr_err("unexpected IRQ trap at vector %02x\n", irq);

	/*
	 * Currently unexpected vectors happen only on SMP and APIC.
	 * We _must_ ack these because every local APIC has only N
	 * irq slots per priority level, and a 'hanging, unacked' IRQ
	 * holds up an irq slot - in excessive cases (when multiple
	 * unexpected vectors occur) that might lock up the APIC
	 * completely.
	 * (当前，异常的中断向量只可能在SMP和APIC下发生。我们必须要针对
	 * 该类中断做回复，因为每个本地APIC只有N个槽位，一个‘挂起，未回应’
	 * 的中断可能会导致该槽位阻塞，当发生过多异常向量的情况我们可能
	 * 需要将APIC彻底锁死)
	 * But only ack when the APIC is enabled -AK
	 * (当时只需要回应使能的APIC)
	 */
	ack_APIC_irq();
}

#define irq_stats(x)		(&per_cpu(irq_stat, x))
/*
 * /proc/interrupts printing for arch specific interrupts
 */
int arch_show_interrupts(struct seq_file *p, int prec)
{
	int j;

	seq_printf(p, "%*s: ", prec, "NMI");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->__nmi_count);
	seq_puts(p, "  Non-maskable interrupts\n");
#ifdef CONFIG_X86_LOCAL_APIC
	seq_printf(p, "%*s: ", prec, "LOC");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->apic_timer_irqs);
	seq_puts(p, "  Local timer interrupts\n");

	seq_printf(p, "%*s: ", prec, "SPU");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_spurious_count);
	seq_puts(p, "  Spurious interrupts\n");
	seq_printf(p, "%*s: ", prec, "PMI");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->apic_perf_irqs);
	seq_puts(p, "  Performance monitoring interrupts\n");
	seq_printf(p, "%*s: ", prec, "IWI");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->apic_irq_work_irqs);
	seq_puts(p, "  IRQ work interrupts\n");
	seq_printf(p, "%*s: ", prec, "RTR");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->icr_read_retry_count);
	seq_puts(p, "  APIC ICR read retries\n");
#endif
	if (x86_platform_ipi_callback) {
		seq_printf(p, "%*s: ", prec, "PLT");
		for_each_online_cpu(j)
			seq_printf(p, "%10u ", irq_stats(j)->x86_platform_ipis);
		seq_puts(p, "  Platform interrupts\n");
	}
#ifdef CONFIG_SMP
	seq_printf(p, "%*s: ", prec, "RES");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_resched_count);
	seq_puts(p, "  Rescheduling interrupts\n");
	seq_printf(p, "%*s: ", prec, "CAL");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_call_count);
	seq_puts(p, "  Function call interrupts\n");
	seq_printf(p, "%*s: ", prec, "TLB");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_tlb_count);
	seq_puts(p, "  TLB shootdowns\n");
#endif
#ifdef CONFIG_X86_THERMAL_VECTOR
	seq_printf(p, "%*s: ", prec, "TRM");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_thermal_count);
	seq_puts(p, "  Thermal event interrupts\n");
#endif
#ifdef CONFIG_X86_MCE_THRESHOLD
	seq_printf(p, "%*s: ", prec, "THR");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_threshold_count);
	seq_puts(p, "  Threshold APIC interrupts\n");
#endif
#ifdef CONFIG_X86_MCE_AMD
	seq_printf(p, "%*s: ", prec, "DFR");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_deferred_error_count);
	seq_puts(p, "  Deferred Error APIC interrupts\n");
#endif
#ifdef CONFIG_X86_MCE
	seq_printf(p, "%*s: ", prec, "MCE");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", per_cpu(mce_exception_count, j));
	seq_puts(p, "  Machine check exceptions\n");
	seq_printf(p, "%*s: ", prec, "MCP");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", per_cpu(mce_poll_count, j));
	seq_puts(p, "  Machine check polls\n");
#endif
#if IS_ENABLED(CONFIG_HYPERV) || defined(CONFIG_XEN)
	if (test_bit(HYPERVISOR_CALLBACK_VECTOR, used_vectors)) {
		seq_printf(p, "%*s: ", prec, "HYP");
		for_each_online_cpu(j)
			seq_printf(p, "%10u ",
				   irq_stats(j)->irq_hv_callback_count);
		seq_puts(p, "  Hypervisor callback interrupts\n");
	}
#endif
	seq_printf(p, "%*s: %10u\n", prec, "ERR", atomic_read(&irq_err_count));
#if defined(CONFIG_X86_IO_APIC)
	seq_printf(p, "%*s: %10u\n", prec, "MIS", atomic_read(&irq_mis_count));
#endif
#ifdef CONFIG_HAVE_KVM
	seq_printf(p, "%*s: ", prec, "PIN");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->kvm_posted_intr_ipis);
	seq_puts(p, "  Posted-interrupt notification event\n");

	seq_printf(p, "%*s: ", prec, "PIW");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ",
			   irq_stats(j)->kvm_posted_intr_wakeup_ipis);
	seq_puts(p, "  Posted-interrupt wakeup event\n");
#endif
	return 0;
}

/*
 * /proc/stat helpers
 */
u64 arch_irq_stat_cpu(unsigned int cpu)
{
	u64 sum = irq_stats(cpu)->__nmi_count;

#ifdef CONFIG_X86_LOCAL_APIC
	sum += irq_stats(cpu)->apic_timer_irqs;
	sum += irq_stats(cpu)->irq_spurious_count;
	sum += irq_stats(cpu)->apic_perf_irqs;
	sum += irq_stats(cpu)->apic_irq_work_irqs;
	sum += irq_stats(cpu)->icr_read_retry_count;
#endif
	if (x86_platform_ipi_callback)
		sum += irq_stats(cpu)->x86_platform_ipis;
#ifdef CONFIG_SMP
	sum += irq_stats(cpu)->irq_resched_count;
	sum += irq_stats(cpu)->irq_call_count;
#endif
#ifdef CONFIG_X86_THERMAL_VECTOR
	sum += irq_stats(cpu)->irq_thermal_count;
#endif
#ifdef CONFIG_X86_MCE_THRESHOLD
	sum += irq_stats(cpu)->irq_threshold_count;
#endif
#ifdef CONFIG_X86_MCE
	sum += per_cpu(mce_exception_count, cpu);
	sum += per_cpu(mce_poll_count, cpu);
#endif
	return sum;
}

u64 arch_irq_stat(void)
{
	u64 sum = atomic_read(&irq_err_count);
	return sum;
}


/*
 * do_IRQ handles all normal device IRQ's (the special
 * SMP cross-CPU interrupts have their own specific
 * handlers).
 */
__visible unsigned int __irq_entry do_IRQ(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	struct irq_desc * desc;
	/* high bit used in ret_from_ code  */
	unsigned vector = ~regs->orig_ax;

	/*
	 * NB: Unlike exception entries, IRQ entries do not reliably
	 * handle context tracking in the low-level entry code.  This is
	 * because syscall entries execute briefly with IRQs on before
	 * updating context tracking state, so we can take an IRQ from
	 * kernel mode with CONTEXT_USER.  The low-level entry code only
	 * updates the context if we came from user mode, so we won't
	 * switch to CONTEXT_KERNEL.  We'll fix that once the syscall
	 * code is cleaned up enough that we can cleanly defer enabling
	 * IRQs.
	 */

	entering_irq();

	/* entering_irq() tells RCU that we're not quiescent.  Check it. */
	RCU_LOCKDEP_WARN(!rcu_is_watching(), "IRQ failed to wake up RCU");

	desc = __this_cpu_read(vector_irq[vector]);

	if (!handle_irq(desc, regs)) {
		ack_APIC_irq();

		if (desc != VECTOR_RETRIGGERED) {
			pr_emerg_ratelimited("%s: %d.%d No irq handler for vector\n",
					     __func__, smp_processor_id(),
					     vector);
		} else {
			__this_cpu_write(vector_irq[vector], VECTOR_UNUSED);
		}
	}

	exiting_irq();

	set_irq_regs(old_regs);
	return 1;
}

/*
 * Handler for X86_PLATFORM_IPI_VECTOR.
 */
void __smp_x86_platform_ipi(void)
{
	inc_irq_stat(x86_platform_ipis);

	if (x86_platform_ipi_callback)
		x86_platform_ipi_callback();
}

__visible void smp_x86_platform_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	__smp_x86_platform_ipi();
	exiting_irq();
	set_irq_regs(old_regs);
}

#ifdef CONFIG_HAVE_KVM
static void dummy_handler(void) {}
static void (*kvm_posted_intr_wakeup_handler)(void) = dummy_handler;

void kvm_set_posted_intr_wakeup_handler(void (*handler)(void))
{
	if (handler)
		kvm_posted_intr_wakeup_handler = handler;
	else
		kvm_posted_intr_wakeup_handler = dummy_handler;
}
EXPORT_SYMBOL_GPL(kvm_set_posted_intr_wakeup_handler);

/*
 * Handler for POSTED_INTERRUPT_VECTOR.
 */
__visible void smp_kvm_posted_intr_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	inc_irq_stat(kvm_posted_intr_ipis);
	exiting_irq();
	set_irq_regs(old_regs);
}

/*
 * Handler for POSTED_INTERRUPT_WAKEUP_VECTOR.
 */
__visible void smp_kvm_posted_intr_wakeup_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	inc_irq_stat(kvm_posted_intr_wakeup_ipis);
	kvm_posted_intr_wakeup_handler();
	exiting_irq();
	set_irq_regs(old_regs);
}
#endif

__visible void smp_trace_x86_platform_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	trace_x86_platform_ipi_entry(X86_PLATFORM_IPI_VECTOR);
	__smp_x86_platform_ipi();
	trace_x86_platform_ipi_exit(X86_PLATFORM_IPI_VECTOR);
	exiting_irq();
	set_irq_regs(old_regs);
}

EXPORT_SYMBOL_GPL(vector_used_by_percpu_irq);

#ifdef CONFIG_HOTPLUG_CPU

/* These two declarations are only used in check_irq_vectors_for_cpu_disable()
 * below, which is protected by stop_machine().  Putting them on the stack
 * results in a stack frame overflow.  Dynamically allocating could result in a
 * failure so declare these two cpumasks as global.
 */
static struct cpumask affinity_new, online_new;

/*
 * This cpu is going to be removed and its vectors migrated to the remaining
 * online cpus.  Check to see if there are enough vectors in the remaining cpus.
 * This function is protected by stop_machine().
 */
int check_irq_vectors_for_cpu_disable(void)
{
	unsigned int this_cpu, vector, this_count, count;
	struct irq_desc *desc;
	struct irq_data *data;
	int cpu;

	this_cpu = smp_processor_id();
	cpumask_copy(&online_new, cpu_online_mask);
	cpumask_clear_cpu(this_cpu, &online_new);

	this_count = 0;
	for (vector = FIRST_EXTERNAL_VECTOR; vector < NR_VECTORS; vector++) {
		desc = __this_cpu_read(vector_irq[vector]);
		if (IS_ERR_OR_NULL(desc))
			continue;
		/*
		 * Protect against concurrent action removal, affinity
		 * changes etc.
		 */
		raw_spin_lock(&desc->lock);
		data = irq_desc_get_irq_data(desc);
		cpumask_copy(&affinity_new,
			     irq_data_get_affinity_mask(data));
		cpumask_clear_cpu(this_cpu, &affinity_new);

		/* Do not count inactive or per-cpu irqs. */
		if (!irq_desc_has_action(desc) || irqd_is_per_cpu(data)) {
			raw_spin_unlock(&desc->lock);
			continue;
		}

		raw_spin_unlock(&desc->lock);
		/*
		 * A single irq may be mapped to multiple cpu's
		 * vector_irq[] (for example IOAPIC cluster mode).  In
		 * this case we have two possibilities:
		 *
		 * 1) the resulting affinity mask is empty; that is
		 * this the down'd cpu is the last cpu in the irq's
		 * affinity mask, or
		 *
		 * 2) the resulting affinity mask is no longer a
		 * subset of the online cpus but the affinity mask is
		 * not zero; that is the down'd cpu is the last online
		 * cpu in a user set affinity mask.
		 */
		if (cpumask_empty(&affinity_new) ||
		    !cpumask_subset(&affinity_new, &online_new))
			this_count++;
	}

	count = 0;
	for_each_online_cpu(cpu) {
		if (cpu == this_cpu)
			continue;
		/*
		 * We scan from FIRST_EXTERNAL_VECTOR to first system
		 * vector. If the vector is marked in the used vectors
		 * bitmap or an irq is assigned to it, we don't count
		 * it as available.
		 *
		 * As this is an inaccurate snapshot anyway, we can do
		 * this w/o holding vector_lock.
		 */
		for (vector = FIRST_EXTERNAL_VECTOR;
		     vector < first_system_vector; vector++) {
			if (!test_bit(vector, used_vectors) &&
			    IS_ERR_OR_NULL(per_cpu(vector_irq, cpu)[vector]))
			    count++;
		}
	}

	if (count < this_count) {
		pr_warn("CPU %d disable failed: CPU has %u vectors assigned and there are only %u available.\n",
			this_cpu, this_count, count);
		return -ERANGE;
	}
	return 0;
}

/* A cpu has been removed from cpu_online_mask.  Reset irq affinities. */
/* 一个cpu从cpu_online_mask中移除了，重设中断affinities   fixup修复 */
void fixup_irqs(void)
{
	unsigned int irq, vector;
	static int warned;
	struct irq_desc *desc;
	struct irq_data *data;
	struct irq_chip *chip;
	int ret;

	//每个CPU的 vector_irq[vector] 对应一个 irq_desc 结构体
	//当CPU 停止运行之后,相应的 irq_desc 需要处理
	for_each_irq_desc(irq, desc) {			//遍历每个 irq_desc{}
		int break_affinity = 0;
		int set_affinity = 1;
		const struct cpumask *affinity;

		if (!desc)
			continue;
		if (irq == 2)
			continue;

		/* interrupt's are disabled at this point */
		/* 此时中断是关闭的 */
		raw_spin_lock(&desc->lock);

		data = irq_desc_get_irq_data(desc);
		affinity = irq_data_get_affinity_mask(data);
		//中断没有对应的处理函数;
		//每CPU中断;
		//中断亲和性是在线cpu的子集;  此时不需要作处理
		if (!irq_has_action(irq) || irqd_is_per_cpu(data) ||
		    cpumask_subset(affinity, cpu_online_mask)) {		//几种不需要处理的情况
			raw_spin_unlock(&desc->lock);
			continue;
		}

		/*
		 * Complete the irq move. This cpu is going down and for
		 * non intr-remapping case, we can't wait till this interrupt
		 * arrives at this cpu before completing the irq move.
		 * 
		 * 完成中断转移
		 */
		irq_force_complete_move(desc);

		if (cpumask_any_and(affinity, cpu_online_mask) >= nr_cpu_ids) {
			//affinity & cpu_online_mask = 0 没有重合的位
			//该情况是异常的,表示相应该中断的cpu从 cpu_online_mask 中移除了
			break_affinity = 1;
			affinity = cpu_online_mask;
		}

		chip = irq_data_get_irq_chip(data/*irq_data*/);
		/*
		 * The interrupt descriptor might have been cleaned up
		 * already, but it is not yet removed from the radix tree
		 *
		 * 这个中断描述符可能已经清理了,但是还没从radix tree中移除
		 */
		if (!chip) {
			raw_spin_unlock(&desc->lock);
			continue;
		}

		if (!irqd_can_move_in_process_context(data) && chip->irq_mask)
			chip->irq_mask(data);		//屏蔽中断

		if (chip->irq_set_affinity) {
			ret = chip->irq_set_affinity(data, affinity, true);
			if (ret == -ENOSPC)
				pr_crit("IRQ %d set affinity failed because there are no available vectors.  "
						"The device assigned to this IRQ is unstable.\n", irq);
		} else {
			if (!(warned++))
				set_affinity = 0;
		}

		/*
		 * We unmask if the irq was not marked masked by the
		 * core code. That respects the lazy irq disable
		 * behaviour.
		 */
		if (!irqd_can_move_in_process_context(data) &&
		    !irqd_irq_masked(data) && chip->irq_unmask)
			chip->irq_unmask(data);		//取消屏蔽

		raw_spin_unlock(&desc->lock);

		if (break_affinity && set_affinity)
			pr_notice("Broke affinity for irq %i\n", irq);
		else if (!set_affinity)
			pr_notice("Cannot set affinity for irq %i\n", irq);
	}

	/*
	 * We can remove mdelay() and then send spurious interrupts to
	 * new cpu targets for all the irqs that were handled previously by
	 * this cpu. While it works, I have seen spurious interrupt messages
	 * (nothing wrong but still...).
	 *
	 * So for now, retain mdelay(1) and check the IRR and then send those
	 * interrupts to new targets as this cpu is already offlined...
	 */
	mdelay(1);

	/*
	 * We can walk the vector array of this cpu without holding
	 * vector_lock because the cpu is already marked !online, so
	 * nothing else will touch it.
	 * 可以在没获取锁的情况下遍历该cpu的向量数组,因为该cpu已经被标记
	 * 为掉线,所以不会有其他地方访问该变量.
	 */
	for (vector = FIRST_EXTERNAL_VECTOR; vector < NR_VECTORS; vector++) {
		unsigned int irr;

		if (IS_ERR_OR_NULL(__this_cpu_read(vector_irq[vector])))
			continue;

		//读取IRR(中断请求寄存器),向 new_target cpu发送中断
		irr = apic_read(APIC_IRR + (vector / 32 * 0x10));
		if (irr  & (1 << (vector % 32))) {
			desc = __this_cpu_read(vector_irq[vector]);

			raw_spin_lock(&desc->lock);
			data = irq_desc_get_irq_data(desc);
			chip = irq_data_get_irq_chip(data);
			if (chip->irq_retrigger) {
				chip->irq_retrigger(data);		//重新触发中断
				__this_cpu_write(vector_irq[vector], VECTOR_RETRIGGERED);
			}
			raw_spin_unlock(&desc->lock);
		}
		//如果不是重触发状态,设置该中断向量无效
		if (__this_cpu_read(vector_irq[vector]) != VECTOR_RETRIGGERED)
			__this_cpu_write(vector_irq[vector], VECTOR_UNUSED);
	}
}
#endif

