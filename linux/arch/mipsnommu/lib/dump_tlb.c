/*
 * Dump R4x00 TLB for debugging purposes.
 *
 * Copyright (C) 1994, 1995 by Waldorf Electronics, written by Ralf Baechle.
 * Copyright (C) 1999 by Silicon Graphics, Inc.
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
	pgd_t	*page_dir, *pgd;
	pmd_t	*pmd;
	pte_t	*pte, page;
	unsigned int addr;
	unsigned long val;

	addr = (unsigned int) address;

	printk("Addr                 == %08x\n", addr);
	printk("tasks->mm.pgd        == %08x\n", (unsigned int) t->mm->pgd);

	page_dir = pgd_offset(t->mm, 0);
	printk("page_dir == %08x\n", (unsigned int) page_dir);

	pgd = pgd_offset(t->mm, addr);
	printk("pgd == %08x, ", (unsigned int) pgd);

	pmd = pmd_offset(pgd, addr);
	printk("pmd == %08x, ", (unsigned int) pmd);

	pte = pte_offset(pmd, addr);
	printk("pte == %08x, ", (unsigned int) pte);

	page = *pte;
	printk("page == %08x\n", (unsigned int) pte_val(page));

	val = pte_val(page);
	if (val & _PAGE_PRESENT) printk("present ");
	if (val & _PAGE_READ) printk("read ");
	if (val & _PAGE_WRITE) printk("write ");
	if (val & _PAGE_ACCESSED) printk("accessed ");
	if (val & _PAGE_MODIFIED) printk("modified ");
	if (val & _PAGE_R4KBUG) printk("r4kbug ");
	if (val & _PAGE_GLOBAL) printk("global ");
	if (val & _PAGE_VALID) printk("valid ");
	printk("\n");
}

void
dump_list_current(void *address)
{
	dump_list_process(current, address);
}

unsigned int
vtop(void *address)
{
	pgd_t	*pgd;
	pmd_t	*pmd;
	pte_t	*pte;
	unsigned int addr, paddr;

	addr = (unsigned long) address;
	pgd = pgd_offset(current->mm, addr);
	pmd = pmd_offset(pgd, addr);
	pte = pte_offset(pmd, addr);
	paddr = (KSEG1 | (unsigned int) pte_val(*pte)) & PAGE_MASK;
	paddr |= (addr & ~PAGE_MASK);

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
