/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 - 2000 by Ralf Baechle
 */
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/version.h>

#include <asm/hardirq.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <asm/softirq.h>
#include <asm/system.h>
#include <asm/uaccess.h>

/*
 * This routine handles page faults.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 */
asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long write,
			      unsigned long address)
{
#ifdef DEBUG
	printk("[%s:%d:%08lx:%ld:%08lx]\n", current->comm, current->pid,
	       address, write, regs->cp0_epc);
#endif

/*
 * Oops. The kernel tried to access some bad page. We'll have to
 * terminate things with extreme prejudice.
 */
	if ((unsigned long) address < PAGE_SIZE) {
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	} else
		printk(KERN_ALERT "Unable to handle kernel access");
	printk(" at virtual address %08lx, epc == %08lx, ra == %08lx\n",
      address, regs->cp0_epc, regs->regs[31]);
	if (!user_mode(regs))
	   die("Oops", regs);
	do_exit(SIGKILL);
}
