/*
 *	linux/mm/mprotect.c
 *
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 *  (C) Copyright 1994 Linus Torvalds
 */
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/shm.h>
#include <linux/mman.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

asmlinkage long sys_mprotect(unsigned long start, size_t len, unsigned long prot)
{
	return -ENOSYS;
}
