/* $Id: mmu_context.h,v 1.1 2001/02/22 19:18:28 serg Exp $
 *
 * Switch a MMU context.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 1999 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_MMU_CONTEXT_H
#define _ASM_MMU_CONTEXT_H

#include <linux/config.h>
#include <asm/pgalloc.h>

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

extern inline void
get_new_mmu_context(struct mm_struct *mm, unsigned long asid)
{
}

/*
 * Initialize the context related info for a new mm_struct
 * instance.
 */
extern inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
//	mm->context = 0;
	return 0;
}

extern inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
                             struct task_struct *tsk, unsigned cpu)
{
}

/*
 * Destroy context related info for an mm_struct that is about
 * to be put to rest.
 */
extern inline void destroy_context(struct mm_struct *mm)
{
	/* Nothing to do.  */
}

/*
 * After we have set current->mm to a new value, this activates
 * the context for the new mm so we see the new mappings.
 */
extern inline void
activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
}

#endif /* _ASM_MMU_CONTEXT_H */
