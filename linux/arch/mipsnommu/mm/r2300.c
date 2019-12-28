/*
 * r2300.c: R2000 and R3000 specific cache code.
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 *
 * with a lot of changes to make this thing work for R3000s
 * Copyright (C) 1998, 2000 Harald Koerfgen
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/system.h>
#include <asm/isadep.h>
#include <asm/io.h>
#include <asm/wbflush.h>
#include <asm/mipsregs.h>

/* Primary cache parameters. */
static unsigned long icache_size, dcache_size; /* Size in bytes */
/* the linesizes are usually fixed on R3000s */

#undef DEBUG_CACHE

/* page functions */
void r3k_clear_page(void * page)
{
	__asm__ __volatile__(
		".set\tnoreorder\n\t"
		".set\tnoat\n\t"
		"addiu\t$1,%0,%2\n"
		"1:\tsw\t$0,(%0)\n\t"
		"sw\t$0,4(%0)\n\t"
		"sw\t$0,8(%0)\n\t"
		"sw\t$0,12(%0)\n\t"
		"addiu\t%0,32\n\t"
		"sw\t$0,-16(%0)\n\t"
		"sw\t$0,-12(%0)\n\t"
		"sw\t$0,-8(%0)\n\t"
		"bne\t$1,%0,1b\n\t"
		"sw\t$0,-4(%0)\n\t"
		".set\tat\n\t"
		".set\treorder"
		:"=r" (page)
		:"0" (page),
		 "I" (PAGE_SIZE)
		:"$1","memory");
}

static void r3k_copy_page(void * to, void * from)
{
	unsigned long dummy1, dummy2;
	unsigned long reg1, reg2, reg3, reg4;

	__asm__ __volatile__(
		".set\tnoreorder\n\t"
		".set\tnoat\n\t"
		"addiu\t$1,%0,%8\n"
		"1:\tlw\t%2,(%1)\n\t"
		"lw\t%3,4(%1)\n\t"
		"lw\t%4,8(%1)\n\t"
		"lw\t%5,12(%1)\n\t"
		"sw\t%2,(%0)\n\t"
		"sw\t%3,4(%0)\n\t"
		"sw\t%4,8(%0)\n\t"
		"sw\t%5,12(%0)\n\t"
		"lw\t%2,16(%1)\n\t"
		"lw\t%3,20(%1)\n\t"
		"lw\t%4,24(%1)\n\t"
		"lw\t%5,28(%1)\n\t"
		"sw\t%2,16(%0)\n\t"
		"sw\t%3,20(%0)\n\t"
		"sw\t%4,24(%0)\n\t"
		"sw\t%5,28(%0)\n\t"
		"addiu\t%0,64\n\t"
		"addiu\t%1,64\n\t"
		"lw\t%2,-32(%1)\n\t"
		"lw\t%3,-28(%1)\n\t"
		"lw\t%4,-24(%1)\n\t"
		"lw\t%5,-20(%1)\n\t"
		"sw\t%2,-32(%0)\n\t"
		"sw\t%3,-28(%0)\n\t"
		"sw\t%4,-24(%0)\n\t"
		"sw\t%5,-20(%0)\n\t"
		"lw\t%2,-16(%1)\n\t"
		"lw\t%3,-12(%1)\n\t"
		"lw\t%4,-8(%1)\n\t"
		"lw\t%5,-4(%1)\n\t"
		"sw\t%2,-16(%0)\n\t"
		"sw\t%3,-12(%0)\n\t"
		"sw\t%4,-8(%0)\n\t"
		"bne\t$1,%0,1b\n\t"
		"sw\t%5,-4(%0)\n\t"
		".set\tat\n\t"
		".set\treorder"
		:"=r" (dummy1), "=r" (dummy2),
		 "=&r" (reg1), "=&r" (reg2), "=&r" (reg3), "=&r" (reg4)
		:"0" (to), "1" (from),
		 "I" (PAGE_SIZE));
}

unsigned long __init r3k_cache_size(unsigned long ca_flags)
{
	unsigned long flags, status, dummy, size;
	volatile unsigned long *p;

	p = (volatile unsigned long *) KUSEG;

	flags = read_32bit_cp0_register(CP0_STATUS);

	/* isolate cache space */
	write_32bit_cp0_register(CP0_STATUS, (ca_flags|flags)&~ST0_IEC);

	*p = 0xa5a55a5a;
	dummy = *p;
	status = read_32bit_cp0_register(CP0_STATUS);

	if (dummy != 0xa5a55a5a || (status & (1<<19))) {
		size = 0;
	} else {
		for (size = 512; size <= 0x40000; size <<= 1)
			*(p + size) = 0;
		*p = -1;
		for (size = 512; 
		     (size <= 0x40000) && (*(p + size) == 0); 
		     size <<= 1)
			;
		if (size > 0x40000)
			size = 0;
	}

	write_32bit_cp0_register(CP0_STATUS, flags);

	return size * sizeof(*p);
}

static void __init probe_dcache(void)
{
//	dcache_size = r3k_cache_size(ST0_ISC);
	dcache_size = 1024;
	printk("Primary data cache %lukb, linesize 4 bytes\n",
		dcache_size >> 10);
}

static void __init probe_icache(void)
{
//	icache_size = r3k_cache_size(ST0_ISC|ST0_SWC);
	icache_size = 4096;
	printk("Primary instruction cache %lukb, linesize 4 bytes\n",
		icache_size >> 10);
}

static inline void r3k_flush_cache_all(void)
{

	__asm__ __volatile__(
		".set\tnoreorder\n\t"
		"mfc0\t$8,$12\n\t"
		"nop\n\t"
		"la\t$26,1f\n\t"
		"lui\t$13,0xA000\n\t"
		"or\t$26,$13\n\t"
		"jr\t$26\n"
      "1:\tli\t$9,0x804\n\t"
		"sw\t$9,0xFFFE0130\n\t"
		"lui\t$12,1\n\t"
		"mtc0\t$12,$12\n\t"
		"nop\n\t"
		"nop\n\t"
		"li\t$10,0\n\t"
		"li\t$11,0xF80\n"
      "1:\tsw\t$0,0($10)\n\t"
		"sw\t$0,0x10($10)\n\t"
		"sw\t$0,0x20($10)\n\t"
		"sw\t$0,0x30($10)\n\t"
		"sw\t$0,0x40($10)\n\t"
		"sw\t$0,0x50($10)\n\t"
		"sw\t$0,0x60($10)\n\t"
		"sw\t$0,0x70($10)\n\t"
		"bne\t$10,$11,1b\n\t"
		"addi\t$10,0x80\n\t"
		"mtc0\t$0,$12\n\t"
		"nop\n\t"
		"li\t$9,0x800\n\t"
		"sw\t$9,0xFFFE0130\n\t"
		"mtc0\t$12,$12\n\t"
		"nop\n\t"
		"nop\n\t"
		"li\t$10,0\n\t"
		"li\t$11,0xF80\n"
      "1:\tsw\t$0,0($10)\n\t"
		"sw\t$0,4($10)\n\t"
		"sw\t$0,8($10)\n\t"
		"sw\t$0,0xC($10)\n\t"
		"sw\t$0,0x10($10)\n\t"
		"sw\t$0,0x14($10)\n\t"
		"sw\t$0,0x18($10)\n\t"
		"sw\t$0,0x1C($10)\n\t"
		"sw\t$0,0x20($10)\n\t"
		"sw\t$0,0x24($10)\n\t"
		"sw\t$0,0x28($10)\n\t"
		"sw\t$0,0x2C($10)\n\t"
		"sw\t$0,0x30($10)\n\t"
		"sw\t$0,0x34($10)\n\t"
		"sw\t$0,0x38($10)\n\t"
		"sw\t$0,0x3C($10)\n\t"
		"sw\t$0,0x40($10)\n\t"
		"sw\t$0,0x44($10)\n\t"
		"sw\t$0,0x48($10)\n\t"
		"sw\t$0,0x4C($10)\n\t"
		"sw\t$0,0x50($10)\n\t"
		"sw\t$0,0x54($10)\n\t"
		"sw\t$0,0x58($10)\n\t"
		"sw\t$0,0x5C($10)\n\t"
		"sw\t$0,0x60($10)\n\t"
		"sw\t$0,0x64($10)\n\t"
		"sw\t$0,0x68($10)\n\t"
		"sw\t$0,0x6C($10)\n\t"
		"sw\t$0,0x70($10)\n\t"
		"sw\t$0,0x74($10)\n\t"
		"sw\t$0,0x78($10)\n\t"
		"sw\t$0,0x7C($10)\n\t"
		"bne\t$10,$11,1b\n\t"
		"addi\t$10,0x80\n\t"
		"mtc0\t$0,$12\n\t"
		"nop\n\t"
		"li\t$9,0x1E988\n\t"
		"sw\t$9,0xFFFE0130\n\t"
		"mtc0\t$8,$12\n\t"
		"nop\n\t"
		".set\treorder");
}

static void r3k_flush_icache_range(unsigned long start, unsigned long end)
{
		r3k_flush_cache_all();
}

static void r3k_flush_dcache_range(unsigned long start, unsigned long end)
{
		r3k_flush_cache_all();
}

static inline unsigned long get_phys_page (unsigned long addr,
					   struct mm_struct *mm)
{
	return PHYSADDR(addr & PAGE_MASK);
}
 
static void r3k_flush_cache_mm(struct mm_struct *mm)
{
		r3k_flush_cache_all();
}

static void r3k_flush_cache_range(struct mm_struct *mm,
				    unsigned long start,
				    unsigned long end)
{
	r3k_flush_cache_all();
}

static void r3k_flush_cache_page(struct vm_area_struct *vma,
				   unsigned long page)
{
	r3k_flush_cache_all();
}

static void r3k_flush_page_to_ram(struct page * page)
{
	/*
	 * Nothing to be done
	 */
}

static void r3k_flush_icache_page(struct vm_area_struct *vma,
				  struct page *page)
{
	unsigned long physpage;

	physpage = (unsigned long) page_address(page);
	if (physpage)
		r3k_flush_icache_range(physpage, physpage + PAGE_SIZE);
}

static void r3k_flush_cache_sigtramp(unsigned long addr)
{
	r3k_flush_cache_all();
}

static void r3k_dma_cache_wback_inv(unsigned long start, unsigned long size)
{
	wbflush();
	r3k_flush_dcache_range(start, start + size);
}

/* TLB operations. */
void flush_tlb_all(void)
{
}

void flush_tlb_mm(struct mm_struct *mm)
{
}

void flush_tlb_range(struct mm_struct *mm, unsigned long start,
				  unsigned long end)
{
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
}

void show_regs(struct pt_regs * regs)
{
	/*
	 * Saved main processor registers
	 */
	printk("$0 : %08x %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       0, (unsigned long) regs->regs[1], (unsigned long) regs->regs[2],
	       (unsigned long) regs->regs[3], (unsigned long) regs->regs[4],
	       (unsigned long) regs->regs[5], (unsigned long) regs->regs[6],
	       (unsigned long) regs->regs[7]);
	printk("$8 : %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       (unsigned long) regs->regs[8], (unsigned long) regs->regs[9],
	       (unsigned long) regs->regs[10], (unsigned long) regs->regs[11],
               (unsigned long) regs->regs[12], (unsigned long) regs->regs[13],
	       (unsigned long) regs->regs[14], (unsigned long) regs->regs[15]);
	printk("$16: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	       (unsigned long) regs->regs[16], (unsigned long) regs->regs[17],
	       (unsigned long) regs->regs[18], (unsigned long) regs->regs[19],
               (unsigned long) regs->regs[20], (unsigned long) regs->regs[21],
	       (unsigned long) regs->regs[22], (unsigned long) regs->regs[23]);
	printk("$24: %08lx %08lx                   %08lx %08lx %08lx %08lx\n",
	       (unsigned long) regs->regs[24], (unsigned long) regs->regs[25],
	       (unsigned long) regs->regs[28], (unsigned long) regs->regs[29],
               (unsigned long) regs->regs[30], (unsigned long) regs->regs[31]);

	/*
	 * Saved cp0 registers
	 */
	printk("epc  : %08lx\nStatus: %08x\nCause : %08x\n",
	       (unsigned long) regs->cp0_epc, (unsigned int) regs->cp0_status,
	       (unsigned int) regs->cp0_cause);
}

void add_wired_entry(unsigned long entrylo0, unsigned long entrylo1,
				  unsigned long entryhi, unsigned long pagemask)
{
printk("r3k_add_wired_entry");
        /*
	 * FIXME, to be done
	 */
}

void __init ld_mmu_r2300(void)
{
	printk("CPU revision is: %08x\n", read_32bit_cp0_register(CP0_PRID));

	_clear_page = r3k_clear_page;
	_copy_page = r3k_copy_page;

	probe_icache();
	probe_dcache();

	_flush_cache_all = r3k_flush_cache_all;
	_flush_cache_mm = r3k_flush_cache_mm;
	_flush_cache_range = r3k_flush_cache_range;
	_flush_cache_page = r3k_flush_cache_page;
	_flush_cache_sigtramp = r3k_flush_cache_sigtramp;
	_flush_page_to_ram = r3k_flush_page_to_ram;
	_flush_icache_page = r3k_flush_icache_page;
	_flush_icache_range = r3k_flush_icache_range;

        _dma_cache_wback_inv = r3k_dma_cache_wback_inv;

}
