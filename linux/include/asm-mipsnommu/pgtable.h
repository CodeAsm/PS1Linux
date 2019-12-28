/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 95, 96, 97, 98, 99, 2000 by Ralf Baechle at alii
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H

#include <asm/addrspace.h>
#include <asm/page.h>

#ifndef _LANGUAGE_ASSEMBLY

#include <linux/linkage.h>
#include <asm/cachectl.h>
#include <linux/config.h>

extern void paging_init (void);

/* Cache flushing:
 *
 *  - flush_cache_all() flushes entire cache
 *  - flush_cache_mm(mm) flushes the specified mm context's cache lines
 *  - flush_cache_page(mm, vmaddr) flushes a single page
 *  - flush_cache_range(mm, start, end) flushes a range of pages
 *  - flush_page_to_ram(page) write back kernel page to ram
 *  - flush_icache_range(start, end) flush a range of instructions
 */
extern void (*_flush_cache_all)(void);
extern void (*_flush_cache_mm)(struct mm_struct *mm);
extern void (*_flush_cache_range)(struct mm_struct *mm, unsigned long start,
				 unsigned long end);
extern void (*_flush_cache_page)(struct vm_area_struct *vma, unsigned long page);
extern void (*_flush_cache_sigtramp)(unsigned long addr);
extern void (*_flush_page_to_ram)(struct page * page);
extern void (*_flush_icache_range)(unsigned long start, unsigned long end);
extern void (*_flush_icache_page)(struct vm_area_struct *vma,
                                  struct page *page);

#define flush_dcache_page(page)			do { } while (0)

#define flush_cache_all()		_flush_cache_all()
#define flush_cache_mm(mm)		_flush_cache_mm(mm)
#define flush_cache_range(mm,start,end)	_flush_cache_range(mm,start,end)
#define flush_cache_page(vma,page)	_flush_cache_page(vma, page)
#define flush_cache_sigtramp(addr)	_flush_cache_sigtramp(addr)
#define flush_page_to_ram(page)		_flush_page_to_ram(page)

#define flush_icache_range(start, end)	_flush_icache_range(start,end)
#define flush_icache_page(vma, page) 	_flush_icache_page(vma, page)

#endif /* !defined (_LANGUAGE_ASSEMBLY) */

#define page_address(page)   ({ if (!(page)->virtual) BUG(); (page)->virtual;})

#define PAGE_NONE		__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_SHARED		__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_COPY		__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_READONLY	__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_KERNEL		__pgprot(0)    /* these mean nothing to NO_MM */

#endif /* _ASM_PGTABLE_H */
