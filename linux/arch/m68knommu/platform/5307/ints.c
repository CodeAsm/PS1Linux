/*
 * linux/arch/m68knommu/platform/5307/ints.c -- General interrupt handling code
 *
 * Copyright (C) 1999  Greg Ungerer (gerg@lineo.com)
 * Copyright (C) 1998  D. Jeff Dionne <jeff@lineo.ca>,
 *                     Kenneth Albanowski <kjahds@kjahds.com>,
 * Copyright (C) 2000  Lineo Inc. (www.lineo.com) 
 *
 * Based on:
 *
 * linux/arch/m68k/kernel/ints.c -- Linux/m68k general interrupt handling code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/config.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/page.h>
#include <asm/machdep.h>

/*
 *	This table stores the address info for each vector handler.
 */
irq_handler_t irq_list[SYS_IRQS];

unsigned int *mach_kstat_irqs;

/* The number of spurious interrupts */
volatile unsigned int num_spurious;

unsigned int local_bh_count[NR_CPUS];
unsigned int local_irq_count[NR_CPUS];

static void default_irq_handler(int irq, void *ptr, struct pt_regs *regs)
{
#if 1
	printk("%s(%d): default irq handler vec=%d [0x%x]\n",
		__FILE__, __LINE__, irq, irq);
#endif
}

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
			irq_list[i].handler = default_irq_handler;
		irq_list[i].flags   = IRQ_FLG_STD;
		irq_list[i].dev_id  = NULL;
		irq_list[i].devname = NULL;
	}

	if (mach_init_IRQ)
		mach_init_IRQ ();
	mach_kstat_irqs = &kstat.irqs[0][0];
}

int request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *dev_id)
{
#if DAVIDM
	if ((irq & IRQ_MACHSPEC) && mach_request_irq) {
		return mach_request_irq(IRQ_IDX(irq), handler, flags,
			devname, dev_id);
	}
#endif

	if (irq < 0 || irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d from %s\n", __FUNCTION__,
			irq, devname);
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

	if (flags & IRQ_FLG_FAST) {
		extern asmlinkage void fasthandler(void);
		extern void set_evector(int vecnum, void (*handler)(void));
		set_evector(irq, fasthandler);
	}

	irq_list[irq].handler = handler;
	irq_list[irq].flags   = flags;
	irq_list[irq].dev_id  = dev_id;
	irq_list[irq].devname = devname;
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
#if DAVIDM
	if (irq & IRQ_MACHSPEC) {
		mach_free_irq(IRQ_IDX(irq), dev_id);
		return;
	}
#endif

	if (irq < 0 || irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (irq_list[irq].dev_id != dev_id)
		printk("%s: Removing probably wrong IRQ %d from %s\n",
		       __FUNCTION__, irq, irq_list[irq].devname);

	if (irq_list[irq].flags & IRQ_FLG_FAST) {
		extern asmlinkage void inthandler(void);
		extern void set_evector(int vecnum, void (*handler)(void));
		set_evector(irq, inthandler);
	}

	if (mach_default_handler)
		irq_list[irq].handler = (*mach_default_handler)[irq];
	else
		irq_list[irq].handler = default_irq_handler;
	irq_list[irq].flags   = IRQ_FLG_STD;
	irq_list[irq].dev_id  = NULL;
	irq_list[irq].devname = NULL;
}


int sys_request_irq(unsigned int irq, 
                    void (*handler)(int, void *, struct pt_regs *), 
                    unsigned long flags, const char *devname, void *dev_id)
{
	if (irq < IRQ1 || irq > IRQ7) {
		printk("%s: Incorrect IRQ %d from %s\n",
		       __FUNCTION__, irq, devname);
		return -ENXIO;
	}

#if 0
	if (!(irq_list[irq].flags & IRQ_FLG_STD)) {
		if (irq_list[irq].flags & IRQ_FLG_LOCK) {
			printk("%s: IRQ %d from %s is not replaceable\n",
			       __FUNCTION__, irq, irq_list[irq].devname);
			return -EBUSY;
		}
		if (!(flags & IRQ_FLG_REPLACE)) {
			printk("%s: %s can't replace IRQ %d from %s\n",
			       __FUNCTION__, devname, irq, irq_list[irq].devname);
			return -EBUSY;
		}
	}
#endif

	irq_list[irq].handler = handler;
	irq_list[irq].flags   = flags;
	irq_list[irq].dev_id  = dev_id;
	irq_list[irq].devname = devname;
	return 0;
}

void sys_free_irq(unsigned int irq, void *dev_id)
{
	if (irq < IRQ1 || irq > IRQ7) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (irq_list[irq].dev_id != dev_id)
		printk("%s: Removing probably wrong IRQ %d from %s\n",
		       __FUNCTION__, irq, irq_list[irq].devname);

	irq_list[irq].handler = (*mach_default_handler)[irq];
	irq_list[irq].flags   = 0;
	irq_list[irq].dev_id  = NULL;
	// irq_list[irq].devname = default_names[irq];
}

/*
 * Do we need these probe functions on the m68k?
 *
 *  ... may be usefull with ISA devices
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
	if (vec >= VEC_INT1 && vec <= VEC_INT7) {
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

	for (i = 0; i < NR_IRQS; i++) {
		if (irq_list[i].flags & IRQ_FLG_STD)
			continue;

		len += sprintf(buf+len, "%3d: %10u ", i,
		               i ? kstat.irqs[0][i] : num_spurious);
		if (irq_list[i].flags & IRQ_FLG_LOCK)
			len += sprintf(buf+len, "L ");
		else
			len += sprintf(buf+len, "  ");
		len += sprintf(buf+len, "%s\n", irq_list[i].devname);
	}

	if (mach_get_irq_list)
		len += mach_get_irq_list(buf+len);
	return len;
}
