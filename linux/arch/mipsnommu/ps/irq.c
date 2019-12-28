/*
 * Code to handle PlayStation IRQs plus some generic interrupt stuff.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/malloc.h>
#include <linux/random.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include <asm/ps/interrupts.h>

unsigned long spurious_count = 0;

volatile extern int * int_ackn_reg;
volatile extern int * int_mask_reg;

static inline void mask_irq(unsigned int irq_nr)
{
   *int_mask_reg &= ~(cpu_mask_tbl[irq_nr] & 0x7ff);
}

static inline void unmask_irq(unsigned int irq_nr)
{
   *int_mask_reg |= (cpu_mask_tbl[irq_nr] & 0x7ff);
}

void disable_irq(unsigned int irq_nr)
{
    unsigned long flags;

    save_and_cli(flags);
    mask_irq(irq_nr);
    restore_flags(flags);
}

void enable_irq(unsigned int irq_nr)
{
    unsigned long flags;

    save_and_cli(flags);
    unmask_irq(irq_nr);
    restore_flags(flags);
}

/*
 * Pointers to the low-level handlers: first the general ones, then the
 * fast ones, then the bad ones.
 */
extern void interrupt(void);

static struct irqaction *irq_action[32] =
{
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

int get_irq_list(char *buf)
{
    int i, len = 0;
    struct irqaction *action;

    for (i = 0; i < 32; i++) {
	action = irq_action[i];
	if (!action)
	    continue;
	len += sprintf(buf + len, "%2d: %8d %c %s",
		       i, kstat.irqs[0][i],
		       (action->flags & SA_INTERRUPT) ? '+' : ' ',
		       action->name);
	for (action = action->next; action; action = action->next) {
	    len += sprintf(buf + len, ",%s %s",
			   (action->flags & SA_INTERRUPT) ? " +" : "",
			   action->name);
	}
	len += sprintf(buf + len, "\n");
    }
    return len;
}

/*
 * do_IRQ handles IRQ's that have been installed without the
 * SA_INTERRUPT flag: it uses the full signal-handling return
 * and runs with other interrupts enabled. All relatively slow
 * IRQ's should use this format: notably the keyboard/timer
 * routines.
 */
asmlinkage void do_IRQ(int irq, struct pt_regs *regs)
{
    struct irqaction *action;
    int do_random, cpu;

    cpu = smp_processor_id();
    irq_enter(cpu, irq);
    kstat.irqs[cpu][irq]++;

    mask_irq(irq);
    action = *(irq + irq_action);
    if (action) {
	if (!(action->flags & SA_INTERRUPT))
	    __sti();
	action = *(irq + irq_action);
	do_random = 0;
	do {
	    do_random |= action->flags;
	    action->handler(irq, action->dev_id, regs);
	    action = action->next;
	} while (action);
	if (do_random & SA_SAMPLE_RANDOM)
	    add_interrupt_randomness(irq);
	unmask_irq(irq);
   
   *int_ackn_reg ^= (cpu_mask_tbl[irq] & 0x7ff);  /* !!! acknowledge interrupt (here ?) !!! */
   
	__cli();
    }
    irq_exit(cpu, irq);

    /* unmasking and bottom half handling is done magically for us. */
}

/*
 * Idea is to put all interrupts
 * in a single table and differenciate them just by number.
 */
int setup_ps_irq(int irq, struct irqaction *new)
{
    int shared = 0;
    struct irqaction *old, **p;
    unsigned long flags;

    p = irq_action + irq;
    if ((old = *p) != NULL) {
	/* Can't share interrupts unless both agree to */
	if (!(old->flags & new->flags & SA_SHIRQ))
	    return -EBUSY;

	/* Can't share interrupts unless both are same type */
	if ((old->flags ^ new->flags) & SA_INTERRUPT)
	    return -EBUSY;

	/* add new interrupt at end of irq queue */
	do {
	    p = &old->next;
	    old = *p;
	} while (old);
	shared = 1;
    }
    if (new->flags & SA_SAMPLE_RANDOM)
	rand_initialize_irq(irq);

    save_and_cli(flags);
    *p = new;

    if (!shared) {
	unmask_irq(irq);
    }
    restore_flags(flags);
    return 0;
}

int request_irq(unsigned int irq,
		void (*handler) (int, void *, struct pt_regs *),
		unsigned long irqflags,
		const char *devname,
		void *dev_id)
{
    int retval;
    struct irqaction *action;

    if (irq >= 32)
	return -EINVAL;
    if (!handler)
	return -EINVAL;

    action = (struct irqaction *) kmalloc(sizeof(struct irqaction), GFP_KERNEL);
    if (!action)
	return -ENOMEM;

    action->handler = handler;
    action->flags = irqflags;
    action->mask = 0;
    action->name = devname;
    action->next = NULL;
    action->dev_id = dev_id;

    retval = setup_ps_irq(irq, action);

    if (retval)
	kfree(action);
    return retval;
}

void free_irq(unsigned int irq, void *dev_id)
{
    struct irqaction *action, **p;
    unsigned long flags;

    if (irq > 39) {
	printk("Trying to free IRQ%d\n", irq);
	return;
    }
    for (p = irq + irq_action; (action = *p) != NULL; p = &action->next) {
	if (action->dev_id != dev_id)
	    continue;

	/* Found it - now free it */
	save_and_cli(flags);
	*p = action->next;
	if (!irq[irq_action])
	    mask_irq(irq);
	restore_flags(flags);
	kfree(action);
	return;
    }
    printk("Trying to free free IRQ%d\n", irq);
}

unsigned long probe_irq_on(void)
{
    /* TODO */
    return 0;
}

int probe_irq_off(unsigned long irqs)
{
    /* TODO */
    return 0;
}

void __init init_IRQ(void)
{
    irq_setup();
}