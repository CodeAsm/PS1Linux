/*
 *  linux/mm/swapfile.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 */

#include <linux/malloc.h>
#include <linux/smp_lock.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/blkdev.h> /* for blk_size */
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/shm.h>

#include <asm/pgtable.h>

asmlinkage long sys_swapoff(const char * specialfile)
{
	return -ENOSYS;
}

int get_swaparea_info(char *buf)
{
	return sprintf(buf, "No swap");
}

int is_swap_partition(kdev_t dev) {
	return 0;
}

asmlinkage long sys_swapon(const char * specialfile, int swap_flags)
{
	return -ENOSYS;
}

void si_swapinfo(struct sysinfo *val)
{
	val->freeswap = val->totalswap = 0;
	return;
}
