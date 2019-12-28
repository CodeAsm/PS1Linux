/*
 *  linux/mmnommu/swap_state.c
 *
 *  Copyright (c) 2000 Lineo, Inc. David McCullough <davidm@lineo.com>
 *
 */

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>

#include <asm/pgtable.h>

struct address_space swapper_space = {
	LIST_HEAD_INIT(swapper_space.clean_pages),
	LIST_HEAD_INIT(swapper_space.dirty_pages),
	LIST_HEAD_INIT(swapper_space.locked_pages),
	0,				/* nrpages	*/
	NULL /* &swap_aops */,
};

void add_to_swap_cache(struct page *page, swp_entry_t entry)
{
	BUG();
}

void __delete_from_swap_cache(struct page *page)
{
	BUG();
}

void delete_from_swap_cache_nolock(struct page *page)
{
	BUG();
}

void delete_from_swap_cache(struct page *page)
{
	BUG();
}

void free_page_and_swap_cache(struct page *page)
{
	BUG();
}

struct page * lookup_swap_cache(swp_entry_t entry)
{
	BUG();
	return((struct page *) 0);
}

struct page * read_swap_cache_async(swp_entry_t entry, int wait)
{
	BUG();
	return((struct page *) 0);
}
