/*
 * Dump R3000 TLB for debugging purposes.
 *
 * Copyright (C) 1994, 1995 by Waldorf Electronics, written by Ralf Baechle.
 * Copyright (C) 1999 by Silicon Graphics, Inc.
 * Copyright (C) 1999 by Harald Koerfgen
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>

#include <asm/bootinfo.h>
#include <asm/cachectl.h>
#include <asm/mipsregs.h>
#include <asm/page.h>
#include <asm/pgtable.h>

void
dump_tlb(int first, int last)
{
}

void
dump_tlb_all(void)
{
}

void
dump_tlb_wired(void)
{
}

void
dump_tlb_addr(unsigned long addr)
{
}

void
dump_tlb_nonwired(void)
{
}

void
dump_list_process(struct task_struct *t, void *address)
{
}

void
dump_list_current(void *address)
{
	dump_list_process(current, address);
}

unsigned int
vtop(void *address)
{
	unsigned int addr, paddr;

	addr = (unsigned long) address;
	paddr = (addr & ~PAGE_MASK);

	return paddr;
}

void
dump16(unsigned long *p)
{
	int i;

	for(i=0;i<8;i++)
	{
		printk("*%08lx == %08lx, ",
		       (unsigned long)p, (unsigned long)*p++);
		printk("*%08lx == %08lx\n",
		       (unsigned long)p, (unsigned long)*p++);
	}
}
