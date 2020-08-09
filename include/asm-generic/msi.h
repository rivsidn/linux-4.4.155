#ifndef __ASM_GENERIC_MSI_H
#define __ASM_GENERIC_MSI_H

#include <linux/types.h>

#ifndef NUM_MSI_ALLOC_SCRATCHPAD_REGS
# define NUM_MSI_ALLOC_SCRATCHPAD_REGS	2
#endif

struct msi_desc;

/**
 * struct msi_alloc_info - Default structure for MSI interrupt allocation.
 * @desc:	Pointer to msi descriptor
 * @hwirq:	Associated hw interrupt number in the domain
 * @scratchpad:	Storage for implementation specific scratch data
 *
 * Architectures can provide their own implementation by not including
 * asm-generic/msi.h into their arch specific header file.
 * (不同架构可以提供他们自己的实现，通过不包含 asm-generic/msi.h 文件到他
 * 们自己的头文件中)
 */
typedef struct msi_alloc_info {
	struct msi_desc			*desc;
	irq_hw_number_t			hwirq;
	union {
		unsigned long		ul;
		void			*ptr;
	} scratchpad[NUM_MSI_ALLOC_SCRATCHPAD_REGS];
} msi_alloc_info_t;

#define GENERIC_MSI_DOMAIN_OPS		1

#endif
