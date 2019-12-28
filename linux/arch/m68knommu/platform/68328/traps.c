/*
 * linux/arch/$(ARCH)/platform/$(PLATFORM)/traps.c -- general exception handling code
 *
 * Cloned from Linux/m68k.
 *
 * No original Copyright holder listed,
 * Probabily original (C) Roman Zippel (assigned DJD, 1999)
 *
 * Copyright 2000-2001 Lineo, Inc. D. Jeff Dionne <jeff@lineo.ca>
 * Copyright 1999-2000 D. Jeff Dionne, <jeff@uclinux.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/page.h>
#include <asm/machdep.h>

/* table for system interrupt handlers */
static irq_handler_t irq_list[SYS_IRQS];

static const char *default_names[SYS_IRQS] = {
	"spurious int", "int1 handler", "int2 handler", "int3 handler",
	"int4 handler", "int5 handler", "int6 handler", "int7 handler"
};

/* The number of spurious interrupts */
volatile unsigned int num_spurious;

#define NUM_IRQ_NODES 16
static irq_node_t nodes[NUM_IRQ_NODES];

#if defined( CONFIG_M68328 ) || defined ( CONFIG_M68EZ328 )
 
asm ("
	.global _start, __ramend
	.section .romvec
 
e_vectors:
	.long __ramend-4, _start, buserr, trap, trap, trap, trap, trap
	.long trap, trap, trap, trap, trap, trap, trap, trap
	.long trap, trap, trap, trap, trap, trap, trap, trap
	.long trap, trap, trap, trap
	.long trap, trap, trap, trap
	/*.long inthandler, inthandler, inthandler, inthandler
	.long inthandler4, inthandler, inthandler, inthandler   */
	/* TRAP #0-15 */
	.long system_call, trap, trap, trap, trap, trap, trap, trap
	.long trap, trap, trap, trap, trap, trap, trap, trap
	.long 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.text

ignore: rte

");
#endif /* CONFIG_M68328 || CONFIG_M68EZ328 */

/*
 * void init_IRQ(void)
 *
 * Parameters:	None
 *
 * Returns:	Nothing
 *
 * This function should be called during kernel startup to initialize
 * the IRQ handling routines.
 */

void init_IRQ(void)
{
	int i;

	for (i = 0; i < SYS_IRQS; i++) {
		if (mach_default_handler)
			irq_list[i].handler = (*mach_default_handler)[i];
		else
			irq_list[i].handler = NULL;
			irq_list[i].flags   = IRQ_FLG_STD;
			irq_list[i].dev_id  = NULL;
			irq_list[i].devname = default_names[i];
	}

	for (i = 0; i < NUM_IRQ_NODES; i++)
		nodes[i].handler = NULL;

	mach_init_IRQ ();
}

irq_node_t *new_irq_node(void)
{
	irq_node_t *node;
	short i;

	for (node = nodes, i = NUM_IRQ_NODES-1; i >= 0; node++, i--)
		if (!node->handler)
			return node;

	printk ("new_irq_node: out of nodes\n");
	return NULL;
}

int request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *dev_id)
{
	if (irq)
		return mach_request_irq(irq, handler, flags, devname, dev_id);

	if (irq < IRQ1 || irq > IRQ7) {
		printk("%s: Incorrect IRQ %d from %s\n", __FUNCTION__, irq, devname);
		return -ENXIO;
	}

	if (!(irq_list[irq].flags & IRQ_FLG_STD)) {
		if (irq_list[irq].flags & IRQ_FLG_LOCK) {
			printk("%s: IRQ %d from %s is not replaceable\n",
			       __FUNCTION__, irq, irq_list[irq].devname);
			return -EBUSY;
		}
		if (flags & IRQ_FLG_REPLACE) {
			printk("%s: %s can't replace IRQ %d from %s\n",
			       __FUNCTION__, devname, irq, irq_list[irq].devname);
			return -EBUSY;
		}
	}
	irq_list[irq].handler = handler;
	irq_list[irq].flags   = flags;
	irq_list[irq].dev_id  = dev_id;
	irq_list[irq].devname = devname;
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	if (irq) {
		mach_free_irq(irq, dev_id);
		return;
	}

	if (irq < IRQ1 || irq > IRQ7) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (irq_list[irq].dev_id != dev_id)
		printk("%s: Removing probably wrong IRQ %d from %s\n",
		       __FUNCTION__, irq, irq_list[irq].devname);

	if (mach_default_handler)
		irq_list[irq].handler = (*mach_default_handler)[irq];
	else
		irq_list[irq].handler = NULL;
	irq_list[irq].flags   = IRQ_FLG_STD;
	irq_list[irq].dev_id  = NULL;
	irq_list[irq].devname = default_names[irq];
}

/*
 * Do we need these probe functions on the m68k?
 */
unsigned long probe_irq_on (void)
{
	return 0;
}

int probe_irq_off (unsigned long irqs)
{
	return 0;
}

asmlinkage void process_int(unsigned long vec, struct pt_regs *fp)
{
        if (vec >= VEC_INT1 && vec <= VEC_INT7 && !MACH_IS_BVME6000) {
                vec -= VEC_SPUR;
                kstat.irqs[0][vec]++;
                irq_list[vec].handler(vec, irq_list[vec].dev_id, fp);
	} else {
                if (mach_process_int)
                        mach_process_int(vec, fp);
                else
                        panic("Can't process interrupt vector %ld\n", vec);
                return;
	}
}

int get_irq_list(char *buf)
{
        int i, len = 0;
	
        /* autovector interrupts */
        if (mach_default_handler) {
                for (i = 0; i < SYS_IRQS; i++) {
                       len += sprintf(buf+len, "auto %2d: %10u ", i,
		       i ? kstat.irqs[0][i] : num_spurious);
                       len += sprintf(buf+len, "  ");
                       len += sprintf(buf+len, "%s\n", irq_list[i].devname);

		}
        }
        len += mach_get_irq_list(buf+len);
        return len;
}

/*
 *	Generic dumping code. Used for panic and debug.
 */

void dump(struct pt_regs *fp)
{
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

	printk("\nCURRENT PROCESS:\n\n");
#if 0
{
	extern int	swt_lastjiffies, swt_reference;
	printk("WATCHDOG: jiffies=%d lastjiffies=%d [%d] reference=%d\n",
		jiffies, swt_lastjiffies, (swt_lastjiffies - jiffies),
		swt_reference);
}
#endif
	printk("COMM=%s PID=%d\n", current->comm, current->pid);
	if (current->mm) {
		printk("TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk("USER-STACK=%08x\n\n",
			(int) current->mm->start_stack);
	}

	printk("PC: %08lx\n", fp->pc);
	printk("SR: %08lx    SP: %08lx\n", (long) fp->sr, (long) fp);
	printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		fp->d0, fp->d1, fp->d2, fp->d3);
	printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		fp->d4, fp->d5, fp->a0, fp->a1);
	printk("\nUSP: %08x   TRAPFRAME: %08x\n",
		rdusp(), (unsigned int) fp);

	printk("\nCODE:");
	tp = ((unsigned char *) fp->pc) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");
/*
       if (STACK_MAGIC != *(unsigned long *)current->kernel_stack_page)
                printk("(Possibly corrupted stack page??)\n");
	printk("\n");
*/
	printk("\nUSER STACK:");
	tp = (unsigned char *) (rdusp() - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");
}
