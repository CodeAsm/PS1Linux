/*
 *	linux/mm/mlock.c
 *
 *  Copyright (c) 2001 Lineo, Inc. David McCullough <davidm@lineo.com>
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 *  (C) Copyright 1995 Linus Torvalds
 */
#include <linux/slab.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/smp_lock.h>
#include <linux/pagemap.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>

asmlinkage long sys_mlock(unsigned long start, size_t len)
{
	return -ENOSYS;
}

asmlinkage long sys_munlock(unsigned long start, size_t len)
{
	return -ENOSYS;
}

static int do_mlockall(int flags)
{
	return -ENOSYS;
}

asmlinkage long sys_mlockall(int flags)
{
	return -ENOSYS;
}

asmlinkage long sys_munlockall(void)
{
	return -ENOSYS;
}
