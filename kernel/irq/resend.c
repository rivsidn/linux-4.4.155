/*
 * linux/kernel/irq/resend.c
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner
 *
 * This file contains the IRQ-resend code
 * (该文件中是中断重发送代码)
 *
 * If the interrupt is waiting to be processed, we try to re-run it.
 * We can't directly run it from here since the caller might be in an
 * interrupt-protected region. Not all irq controller chips can
 * retrigger interrupts at the hardware level, so in those cases
 * we allow the resending of IRQs via a tasklet.
 * (如果中断等待被处理我们尝试重新运行它。
 * 我们不能在此处直接重新运行他们因为调用者可能处于中断保护临界区。
 * 不是所有的中断控制器芯片支持在硬件层面重新触发中断功能，所以在
 * 这种情况下我需要通过tasklet重新发送中断)
 */

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/interrupt.h>

#include "internals.h"

#ifdef CONFIG_HARDIRQS_SW_RESEND

/* Bitmap to handle software resend of interrupts: */
static DECLARE_BITMAP(irqs_resend, IRQ_BITMAP_BITS);

/*
 * Run software resends of IRQ's
 * (通过软件重新发送中断)
 */
static void resend_irqs(unsigned long arg)
{
	struct irq_desc *desc;
	int irq;

	while (!bitmap_empty(irqs_resend, nr_irqs)) {
		irq = find_first_bit(irqs_resend, nr_irqs);
		clear_bit(irq, irqs_resend);
		desc = irq_to_desc(irq);
		local_irq_disable();
		desc->handle_irq(desc);
		local_irq_enable();
	}
}

/* Tasklet to handle resend: */
static DECLARE_TASKLET(resend_tasklet, resend_irqs, 0);

#endif

/*
 * IRQ resend
 *
 * Is called with interrupts disabled and desc->lock held.
 * (被调用时，必须关中断并且获取到了desc->lock锁)
 *
 * (TODO:从 __enable_irq() 中调用时是不是不符合条件？)
 */
void check_irq_resend(struct irq_desc *desc)
{
	/*
	 * We do not resend level type interrupts. Level type
	 * interrupts are resent by hardware when they are still
	 * active. Clear the pending bit so suspend/resume does not
	 * get confused.
	 * 我们不会重新发送level类型中断。
	 * level型中断会由硬件重新发送，如果中断人然处于激活状态，
	 * 清空挂起标识位，所以挂起/恢复不会迷惑。
	 */
	if (irq_settings_is_level(desc)) {
		desc->istate &= ~IRQS_PENDING;
		return;
	}
	if (desc->istate & IRQS_REPLAY)
		return;
	if (desc->istate & IRQS_PENDING) {
		desc->istate &= ~IRQS_PENDING;
		desc->istate |= IRQS_REPLAY;

		if (!desc->irq_data.chip->irq_retrigger ||
		    !desc->irq_data.chip->irq_retrigger(&desc->irq_data)) {
#ifdef CONFIG_HARDIRQS_SW_RESEND	//基于tasklet的硬件中断重新发送机制
			unsigned int irq = irq_desc_get_irq(desc);

			/*
			 * If the interrupt is running in the thread
			 * context of the parent irq we need to be
			 * careful, because we cannot trigger it
			 * directly.
			 * TODO：没理解
			 */
			if (irq_settings_is_nested_thread(desc)) {
				/*
				 * If the parent_irq is valid, we
				 * retrigger the parent, otherwise we
				 * do nothing.
				 */
				if (!desc->parent_irq)
					return;
				irq = desc->parent_irq;
			}
			/* Set it pending and activate the softirq: */
			set_bit(irq, irqs_resend);
			tasklet_schedule(&resend_tasklet);
#endif
		}
	}
}
