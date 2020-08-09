
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


```
void irq_domain_set_info(struct irq_domain *domain, unsigned int virq,
			 irq_hw_number_t hwirq, struct irq_chip *chip,
			 void *chip_data, irq_flow_handler_t handler,
			 void *handler_data, const char *handler_name)

```



## QUES
### manage.c
1. struct irqaction {} 中的 secondary 是做什么用的？
2. forced thread 是什么意思？

## TODO
