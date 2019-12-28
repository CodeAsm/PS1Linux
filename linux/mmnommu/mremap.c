/*
 *	linux/mm/remap.c
 *
 *  Copyright (c) 2001 Lineo, Inc. David McCullough <davidm@lineo.com>
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 *	(C) Copyright 1996 Linus Torvalds
 */

#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/swap.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

/*
 * FIXME: Could do a tradional realloc() in some cases.
 */
asmlinkage unsigned long sys_mremap(unsigned long addr,
	unsigned long old_len, unsigned long new_len,
	unsigned long flags, unsigned long new_addr)
{
	return -ENOSYS;
}
