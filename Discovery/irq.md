
`early_irq_init()`-->`arch_early_irq_init()`-->



`__handle_domain_irq()`-->`irq_enter()`

```
struct irq_desc *desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);

static inline struct irq_desc *
irq_get_desc_lock(unsigned int irq, unsigned long *flags, unsigned int check)
{
	return __irq_get_desc_lock(irq, flags, false, check);
}

#define IRQ_GET_DESC_CHECK_GLOBAL   (_IRQ_DESC_CHECK)
```

## TODO
kernel/irq/manage.c +797


