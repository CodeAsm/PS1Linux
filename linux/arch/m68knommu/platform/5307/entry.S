/* -*- mode: asm -*-
 *
 *  linux/arch/m68knommu/platform/5307/entry.S
 *
 *  Copyright (C) 1999  Greg Ungerer (gerg@lineo.com)
 *  Copyright (C) 1998  D. Jeff Dionne <jeff@lineo.ca>,
 *                      Kenneth Albanowski <kjahds@kjahds.com>,
 *  Copyright (C) 2000  Lineo Inc. (www.lineo.com) 
 *
 * Based on:
 *
 *  linux/arch/m68k/kernel/entry.S
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file README.legal in the main directory of this archive
 * for more details.
 *
 * Linux/m68k support by Hamish Macdonald
 *
 * 68060 fixes by Jesper Skov
 * ColdFire support by Greg Ungerer (gerg@lineo.com)
 * 5307 fixes by David W. Miller
 * linux 2.4 support David McCullough <davidm@lineo.com>
 */

/*
 * entry.S  contains the system-call and fault low-level handling routines.
 * This also contains the timer-interrupt handler, as well as all interrupts
 * and faults that can result in a task-switch.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 * Stack layout in 'ret_from_exception':
 *
 *	This allows access to the syscall arguments in registers d1-d5
 *
 *	 0(sp) - d1
 *	 4(sp) - d2
 *	 8(sp) - d3
 *	 C(sp) - d4
 *	10(sp) - d5
 *	14(sp) - a0
 *	18(sp) - a1
 *	1C(sp) - a2
 *	20(sp) - d0
 *	24(sp) - orig_d0
 *	28(sp) - stack adjustment
 *	2C(sp) - format & vector      }
 *	2E(sp) - sr                   } different to m68k
 *	30(sp) - pc                   }
 */

#include <linux/sys.h>
#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/setup.h>
#include <asm/segment.h>

#include "m68k_defs.h"

LENOSYS = 38

/* the following macro is used when enabling interrupts */
#define ALLOWINT 0xf8ff
#define	MAX_NOINT_IPL	0


LD0			= 0x20
LORIG_D0	= 0x24
LFORMATVEC	= 0x2c
LSR			= 0x2e
LPC			= 0x30

/*
 * This defines the normal kernel pt-regs layout.
 *
 * regs are a2-a6 and d6-d7 preserved by C code
 * the kernel doesn't mess with usp unless it needs to
 *
 *	This is made a little more tricky on the ColdFire. There is no
 *	separate kernel and user stack pointers. Need to artificially
 *	construct a usp in software... When doing this we need to disable
 *	interrupts, otherwise bad things could happen.
 */
#define SAVE_ALL						\
	move	#0x2700,%sr;		/* disable intrs */	\
	btst	#5,%sp@(2);		/* from user? */	\
	bnes	1f;			/* no, skip */		\
	movel	%sp,sw_usp;		/* save user sp */	\
	addql	#8,sw_usp;		/* remove exception */	\
	movel	sw_ksp,%sp;		/* kernel sp */		\
	subql	#8,%sp;			/* room for exception */\
	clrl	%sp@-;			/* stk_adj */		\
	movel	%d0,%sp@-;		/* orig d0 */		\
	movel	%d0,%sp@-;		/* d0 */		\
	subl	#32,%sp;		/* space for 8 regs */	\
	moveml	%d1-%d5/%a0-%a2,%sp@;				\
	movel	sw_usp,%a0;		/* get usp */		\
	moveml	%a0@(-8),%d1-%d2;	/* get exception */	\
	moveml	%d1-%d2,%sp@(LFORMATVEC); /* copy exception */	\
	bra	2f;						\
	1:							\
	clrl	%sp@-;			/* stk_adj */		\
	movel	%d0,%sp@-;		/* orig d0 */		\
	movel	%d0,%sp@-;		/* d0 */		\
	subl	#32,%sp;		/* space for 7 regs */	\
	moveml	%d1-%d5/%a0-%a2,%sp@;				\
	2:

#define RESTORE_ALL						\
	btst	#5,%sp@(LSR);		/* going user? */	\
	bnes	1f;			/* no, skip */		\
	move	#0x2700,%sr;		/* disable intrs */	\
	movel	sw_usp,%a0;		/* get usp */		\
	moveml	%sp@(LFORMATVEC),%d1-%d2; /* copy exception */	\
	moveml	%d1-%d2,%a0@(-8);				\
	moveml	%sp@,%d1-%d5/%a0-%a2;				\
	addl	#32,%sp;		/* space for 8 regs */	\
	movel	%sp@+,%d0;					\
	addql	#4,%sp;			/* orig d0 */		\
	addl	%sp@+,%sp;		/* stk adj */		\
	addql	#8,%sp;			/* remove exception */	\
	movel	%sp,sw_ksp;		/* save ksp */		\
	subql	#8,sw_usp;		/* set exception */	\
	movel	sw_usp,%sp;		/* restore usp */	\
	rte;							\
	1:							\
	moveml	%sp@,%d1-%d5/%a0-%a2;				\
	addl	#32,%sp;		/* space for 8 regs */	\
	movel	%sp@+,%d0;					\
	addql	#4,%sp;			/* orig d0 */		\
	addl	%sp@+,%sp;		/* stk adj */		\
	rte

/*
 *	Quick exception save, use current stack only.
 */
#define SAVE_LOCAL						\
	move	#0x2700,%sr;		/* disable intrs */	\
	clrl	%sp@-;			/* stk_adj */		\
	movel	%d0,%sp@-;		/* orig d0 */		\
	movel	%d0,%sp@-;		/* d0 */		\
	subl	#32,%sp;		/* space for 8 regs */	\
	moveml	%d1-%d5/%a0-%a2,%sp@;

#define RESTORE_LOCAL						\
	moveml	%sp@,%d1-%d5/%a0-%a2;				\
	addl	#32,%sp;		/* space for 8 regs */	\
	movel	%sp@+,%d0;					\
	addql	#4,%sp;			/* orig d0 */		\
	addl	%sp@+,%sp;		/* stk adj */		\
	rte


#define SWITCH_STACK_SIZE (6*4+4)	/* includes return address */

#define SAVE_SWITCH_STACK		\
	subl    #24,%sp; /* 6 regs */	\
	moveml	%a3-%a6/%d6-%d7,%sp@

#define RESTORE_SWITCH_STACK		\
	moveml	%sp@,%a3-%a6/%d6-%d7;	\
	addl	#24,%sp /* 6 regs */

/*
 *	Software copy of the user and kernel stack pointers... Ugh...
 *	Need these to get around ColdFire not having separate kernel
 *	and user stack pointers.
 */
.globl SYMBOL_NAME(sw_usp)
.globl SYMBOL_NAME(sw_ksp)

.data

sw_ksp:
.long	0

sw_usp:
.long	0

.text


.globl SYMBOL_NAME(buserr)
.globl SYMBOL_NAME(trap)
.globl SYMBOL_NAME(system_call)
.globl SYMBOL_NAME(resume), SYMBOL_NAME(ret_from_exception)
.globl SYMBOL_NAME(ret_from_signal)
.globl SYMBOL_NAME(sys_call_table)
.globl SYMBOL_NAME(sys_fork), SYMBOL_NAME(sys_clone), SYMBOL_NAME(sys_vfork)
.globl SYMBOL_NAME(ret_from_interrupt)
.globl SYMBOL_NAME(inthandler)
.globl SYMBOL_NAME(fasthandler)

#ifdef TRAP_DBG_INTERRUPT
.globl SYMBOL_NAME(dbginterrupt)
#endif

.text

ENTRY(buserr)
	SAVE_ALL
	moveq	#-1,%d0
	movel	%d0,%sp@(LORIG_D0)	| a -1 in the ORIG_D0 field
								| signifies that the stack frame
								| is NOT for syscall

	movel	%sp,%sp@- 			| stack frame pointer argument
	jsr	SYMBOL_NAME(buserr_c)
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)


#ifdef TRAP_DBG_INTERRUPT
ENTRY(dbginterrupt)
	SAVE_ALL
	moveq	#-1,%d0
	movel	%d0,%sp@(LORIG_D0)	| a -1 in the ORIG_D0 field
								| signifies that the stack frame
								| is NOT for syscall

	movel	%sp,%sp@- 			| stack frame pointer argument
	jsr	SYMBOL_NAME(dbginterrupt_c)
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)
#endif


ENTRY(reschedule)
	| save top of frame
	pea	%sp@
	jbsr	SYMBOL_NAME(set_esp0)
	addql	#4,%sp

	pea	SYMBOL_NAME(ret_from_exception)
	jmp	SYMBOL_NAME(schedule)

	| After a fork we jump here directly from resume,
	| so that %d1 contains the previous task
	| Theoretically only needed on SMP, but lets watch
	| what happens in schedule_tail() in future...
ENTRY(ret_from_fork)
	movel	%d1,%sp@-
	jsr	SYMBOL_NAME(schedule_tail)
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)

ENTRY(system_call)
	SAVE_ALL
	move	#0x2000,%sr;		| enable intrs again

	movel	#-LENOSYS,%d2
	movel	%d2,LD0(%sp)		| default return value in d0
					| original D0 is in orig_d0
	movel	%d0,%d2

	| save top of frame
	pea	%sp@
	jbsr	SYMBOL_NAME(set_esp0)
	addql	#4,%sp

	cmpl	#NR_syscalls,%d2
	jcc	SYMBOL_NAME(ret_from_exception)
	lea	SYMBOL_NAME(sys_call_table),%a0
	lsll	#2,%d2		| movel	%a0@(%d2:l:4),%d3
	movel	%a0@(%d2),%d3
	jeq	SYMBOL_NAME(ret_from_exception)
	lsrl	#2,%d2
	movel	SYMBOL_NAME(_current_task),%a0
	btst	#1,%a0@(TASK_PTRACE+3)	| PT_TRACESYS
	bnes	1f
	movel	%d3,%a0
	jbsr	%a0@
	movel	%d0,%sp@(LD0)		| save the return value
	jra	SYMBOL_NAME(ret_from_exception)
1:
	subql	#4,%sp
	SAVE_SWITCH_STACK
	jbsr	SYMBOL_NAME(syscall_trace)
	RESTORE_SWITCH_STACK
	addql	#4,%sp
	movel	%d3,%a0
	jbsr	%a0@
	movel	%d0,%sp@(LD0)		| save the return value
	subql	#4,%sp			| dummy return address
	SAVE_SWITCH_STACK
	jbsr	SYMBOL_NAME(syscall_trace)

SYMBOL_NAME_LABEL(ret_from_signal)
	RESTORE_SWITCH_STACK
	addql	#4,%sp

SYMBOL_NAME_LABEL(ret_from_exception)
	btst	#5,%sp@(LSR)		| check if returning to kernel
	bnes	2f			| if so, skip resched, signals
	| tstl	SYMBOL_NAME(need_resched)
	| jne	SYMBOL_NAME(reschedule)
	movel	SYMBOL_NAME(_current_task),%a0
	tstl	%a0@(TASK_NEEDRESCHED)
	jne	SYMBOL_NAME(reschedule)

#if 0 /* as per m68k */
	cmpl	#SYMBOL_NAME(task),%a0	| task[0] cannot have signals
	jeq	2f
#endif

	bclr	#2,%a0@(TASK_PTRACE+3)	| check for delayed trace
	jeq	1f
	bclr	#7,%sp@(LSR)		| clear trace bit in SR
	pea	1			| send SIGTRAP
	movel	%a0,%sp@-
	pea	5
	jbsr	SYMBOL_NAME(send_sig)
	addql	#8,%sp
	addql	#4,%sp
	movel	SYMBOL_NAME(_current_task),%a0

1:
	tstl	%a0@(TASK_STATE)	| state
	jne	SYMBOL_NAME(reschedule)
	tstl	%a0@(TASK_COUNTER)	| counter
	jeq	SYMBOL_NAME(reschedule)

	movel	%a0@(TASK_BLOCKED),%d0
	movel	%d0,%d1			| save blocked in d1 for sig handling
	notl	%d0
	btst	#0,%a0@(TASK_PTRACE+3)	| PT_PTRACED
	jeq	1f
	moveq	#-1,%d0			| let the debugger see all signals
1:	andl	%a0@(TASK_SIGPENDING),%d0
	jne	Lsignal_return
2:	RESTORE_ALL			| Does RTE

Lsignal_return:
	subql	#4,%sp			| dummy return address
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	movel	%d1,%sp@-
	jsr	SYMBOL_NAME(do_signal)
	addql	#8,%sp
	RESTORE_SWITCH_STACK
	addql	#4,%sp

	btst    #5,%sp@(LSR);	/* going user? */   
	bnes    not_user;	/* no, skip */      
	move    #0x2700,%sr;	/* disable intrs */ 
	movel   sw_usp,%a0;	/* get usp */       
	moveml  %sp@(LFORMATVEC),%d1-%d2; /* copy exception */  
	moveml  %d1-%d2,%a0@(-8);               
	bclr    #5,%a0@(-8);  /* clear format byte, bit 5 to make stack appear modulo 4 which it WILL be when we do the rte because it was generated in setup_frame */
	bclr    #4,%a0@(-8);  /* clear format byte, bit 4 to make stack appear modulo 4 which it WILL be when we do the rte because it was generated in setup_frame */
	moveml  %sp@,%d1-%d5/%a0-%a2;               
	addl    #32,%sp;        /* space for 8 regs */  
	movel   %sp@+,%d0;                  
	addql   #4,%sp;         /* orig d0 */       
	addl    %sp@+,%sp;      /* stk adj */       
	addql   #8,%sp;         /* remove exception */  
	movel   %sp,sw_ksp;     /* save ksp */      
	movel   sw_usp,%sp;     /* restore usp */   
	subql   #8,%sp;         /* set exception */ 
	rte;                            
not_user:                          
	moveml  %sp@,%d1-%d5/%a0-%a2;               
	addl    #32,%sp;        /* space for 8 regs */  
	movel   %sp@+,%d0;                  
	addql   #4,%sp;         /* orig d0 */       
	addl    %sp@+,%sp;      /* stk adj */       
	rte

/*--------------------------------------------------------------------------*/

/*
 *	Common ColdFire trap handler. Most traps come through here first.
 */
ENTRY(trap)
	SAVE_ALL
	moveq	#-1,%d0
	movel	%d0,%sp@(LORIG_D0)	| a -1 in the ORIG_D0 field
					| signifies that the stack frame
					| is NOT for syscall
	movel	%sp,%sp@- 		| stack frame pointer argument
	jsr	SYMBOL_NAME(trap_c)
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)

/*
 *	This is the generic interrupt handler (for all hardware interrupt
 *	sources). It figures out the vector number and calls the appropriate
 *	interrupt service routine directly.
 */
SYMBOL_NAME_LABEL(inthandler)
	SAVE_ALL
	moveq	#-1,%d0
	movel	%d0,%sp@(LORIG_D0)	| a -1 in the ORIG_D0 field
					| signifies that the stack frame
					| is NOT for syscall
	addql	#1,SYMBOL_NAME(local_irq_count)
					| put exception # in d0
	movew   %sp@(LFORMATVEC),%d0
	andl	#0x03fc,%d0		| mask out vector only

	movel	SYMBOL_NAME(mach_kstat_irqs),%a0
					| get addr of kstat struct
	addql	#1,%a0@(%d0)		| incr irq intr count

	lsrl	#2,%d0			| calculate real vector #
	movel	%d0,%d1			| calculate array offset
	lsll	#4,%d1
	lea	SYMBOL_NAME(irq_list),%a0
	addl	%d1,%a0			| pointer to array struct

	movel	%sp,%sp@-		| push regs arg onto stack
	movel	%a0@(8),%sp@-		| push devid arg
	movel	%d0,%sp@- 		| push vector # on stack

	movel	%a0@,%a0		| get function to call
	jbsr	%a0@			| call vector handler
	addl	#12,%sp			| pop parameters off stack

	bra	ret_from_interrupt	| this was fallthrough


/*
 *	This is the fast interrupt handler (for certain hardware interrupt
 *	sources). Unlike the normal interrupt handler it just uses the
 *	current stack (doesn't care if it is user or kernel). It also
 *	doesn't bother doing the bottom half handlers.
 */
SYMBOL_NAME_LABEL(fasthandler)
	SAVE_LOCAL

	movew   %sp@(LFORMATVEC),%d0
	andl	#0x03fc,%d0		| mask out vector only

	movel	SYMBOL_NAME(mach_kstat_irqs),%a0
					| get addr of kstat struct
	addql	#1,%a0@(%d0)		| incr irq intr count

	movel	%sp,%sp@-		| push regs arg onto stack
	clrl	%d1
	movel	%d1,%sp@- 		| push devid arg
	lsrl	#2,%d0			| calculate real vector #
	movel	%d0,%sp@- 		| push vector # on stack

	lsll	#4,%d0			| adjust for array offset
	lea	SYMBOL_NAME(irq_list),%a0
	movel	%a0@(%d0),%a0		| get function to call
	jbsr	%a0@			| call vector handler
	addl	#12,%sp			| pop parameters off stack

	RESTORE_LOCAL

/*--------------------------------------------------------------------------*/

SYMBOL_NAME_LABEL(ret_from_interrupt)
	subql	#1,SYMBOL_NAME(local_irq_count)
	jeq	2f
1:
	RESTORE_ALL
2:
#if 1
	|bfextu  %sp@(PT_SR){#5,#3},%d0	| Check for nested interrupt.
	moveb	%sp@(LSR),%d0
	andl	#0x7,%d0
#if MAX_NOINT_IPL > 0
	cmpiw	#MAX_NOINT_IPL,%d0
#endif
	jhi	1b
#endif

	/* check if we need to do software interrupts */

	movel	SYMBOL_NAME(irq_stat)+CPUSTAT_SOFTIRQ_ACTIVE,%d0
	andl	SYMBOL_NAME(irq_stat)+CPUSTAT_SOFTIRQ_MASK,%d0
	jeq	SYMBOL_NAME(ret_from_exception)

	pea	SYMBOL_NAME(ret_from_exception)
	jmp	SYMBOL_NAME(do_softirq)

/* Handler for uninitialized and spurious interrupts */

SYMBOL_NAME_LABEL(bad_interrupt)
	addql	#1,SYMBOL_NAME(num_spurious)
	rte

ENTRY(sys_fork)
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	jbsr	SYMBOL_NAME(m68k_fork)
	addql	#4,%sp
	RESTORE_SWITCH_STACK
	rts

ENTRY(sys_vfork)
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	jbsr	SYMBOL_NAME(m68k_vfork)
	addql	#4,%sp
	RESTORE_SWITCH_STACK
	rts

ENTRY(sys_clone)
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	jbsr	SYMBOL_NAME(m68k_clone)
	addql	#4,%sp
	RESTORE_SWITCH_STACK
	rts

ENTRY(sys_sigsuspend)
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	jbsr	SYMBOL_NAME(do_sigsuspend)
	addql	#4,%sp
	RESTORE_SWITCH_STACK
	rts

ENTRY(sys_rt_sigsuspend)
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	jbsr	SYMBOL_NAME(do_rt_sigsuspend)
	addql	#4,%sp
	RESTORE_SWITCH_STACK
	rts

ENTRY(sys_sigreturn)
	SAVE_SWITCH_STACK
	jbsr	SYMBOL_NAME(do_sigreturn)
	RESTORE_SWITCH_STACK
	rts

ENTRY(sys_rt_sigreturn)
	SAVE_SWITCH_STACK
	jbsr	SYMBOL_NAME(do_rt_sigreturn)
	RESTORE_SWITCH_STACK
	rts

SYMBOL_NAME_LABEL(resume)
	/*
	 * Beware - when entering resume, prev (the current task) is
	 * in a0, next (the new task) is in a1,so don't change these
	 * registers until their contents are no longer needed.
	 */

	/* save sr */
	movew	%sr,%d0
	movew	%d0,%a0@(TASK_THREAD+THREAD_SR)

	/* disable interrupts */
	oril	#0x700,%d0
	move	%d0,%sr

#ifndef NO_MM
	/* save fs (sfc,%dfc) (may be pointing to kernel memory) */
	movec	%sfc,%d0
	movew	%d0,%a0@(TASK_THREAD+THREAD_FS)
#endif

#ifndef CONFIG_COLDFIRE
	/* save usp */
	/* it is better to use a movel here instead of a movew 8*) */
	movec	%usp,%d0
	movel	%d0,%a0@(TASK_THREAD+THREAD_USP)
#else
	movel	sw_usp,%d0 /* usp */
	movel	%d0,%a0@(TASK_THREAD+THREAD_USP) /* DAVIDM was a2 */
#endif

	/* save non-scratch registers on stack */
	SAVE_SWITCH_STACK

	/* save current kernel stack pointer */
	movel	%sp,%a0@(TASK_THREAD+THREAD_KSP)

	/* Return previous task in %d1 */
	// movel	%curptr,%d1
	movel	SYMBOL_NAME(_current_task), %d1

	/* switch to new task (a1 contains new task) */
	// movel	%a1,%curptr
	movel	%a1,SYMBOL_NAME(_current_task)

	/* restore the kernel stack pointer */
	movel	%a1@(TASK_THREAD+THREAD_KSP),%sp

	/* restore non-scratch registers */
	RESTORE_SWITCH_STACK

#ifndef CONFIG_COLDFIRE
	/* restore user stack pointer */
	movel	%a1@(TASK_THREAD+THREAD_USP),%a0
	movel	%a0,%usp
#else
	/* restore user stack pointer */
	movel	%a1@(TASK_THREAD+THREAD_USP),%a0
	movel	%a0, sw_usp
#endif

#ifndef CONFIG_COLDFIRE
	/* restore fs (sfc,%dfc) */
	movew	%a1@(TASK_THREAD+THREAD_FS),%a0
	movec	%a0,%sfc
	movec	%a0,%dfc
#endif

	/* restore status register */
	movew	%a1@(TASK_THREAD+THREAD_SR),%d0
	movew	%d0, %sr

	rts

.text | DAVIDM was .data,  moved to .text
ALIGN
SYMBOL_NAME_LABEL(sys_call_table)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 0  -  old "setup()" system call*/
	.long SYMBOL_NAME(sys_exit)
	.long SYMBOL_NAME(sys_fork)
	.long SYMBOL_NAME(sys_read)
	.long SYMBOL_NAME(sys_write)
	.long SYMBOL_NAME(sys_open)			/* 5 */
	.long SYMBOL_NAME(sys_close)
	.long SYMBOL_NAME(sys_waitpid)
	.long SYMBOL_NAME(sys_creat)
	.long SYMBOL_NAME(sys_link)
	.long SYMBOL_NAME(sys_unlink)		/* 10 */
	.long SYMBOL_NAME(sys_execve)
	.long SYMBOL_NAME(sys_chdir)
	.long SYMBOL_NAME(sys_time)
	.long SYMBOL_NAME(sys_mknod)
	.long SYMBOL_NAME(sys_chmod)		/* 15 */
	.long SYMBOL_NAME(sys_chown16)
	.long SYMBOL_NAME(sys_ni_syscall)				/* old break syscall holder */
	.long SYMBOL_NAME(sys_stat)
	.long SYMBOL_NAME(sys_lseek)
	.long SYMBOL_NAME(sys_getpid)		/* 20 */
	.long SYMBOL_NAME(sys_mount)
	.long SYMBOL_NAME(sys_oldumount)
	.long SYMBOL_NAME(sys_setuid16)
	.long SYMBOL_NAME(sys_getuid16)
	.long SYMBOL_NAME(sys_stime)		/* 25 */
	.long SYMBOL_NAME(sys_ptrace)
	.long SYMBOL_NAME(sys_alarm)
	.long SYMBOL_NAME(sys_fstat)
	.long SYMBOL_NAME(sys_pause)
	.long SYMBOL_NAME(sys_utime)		/* 30 */
	.long SYMBOL_NAME(sys_ni_syscall)				/* old stty syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)				/* old gtty syscall holder */
	.long SYMBOL_NAME(sys_access)
	.long SYMBOL_NAME(sys_nice)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 35 */	/* old ftime syscall holder */
	.long SYMBOL_NAME(sys_sync)
	.long SYMBOL_NAME(sys_kill)
	.long SYMBOL_NAME(sys_rename)
	.long SYMBOL_NAME(sys_mkdir)
	.long SYMBOL_NAME(sys_rmdir)		/* 40 */
	.long SYMBOL_NAME(sys_dup)
	.long SYMBOL_NAME(sys_pipe)
	.long SYMBOL_NAME(sys_times)
	.long SYMBOL_NAME(sys_ni_syscall)				/* old prof syscall holder */
	.long SYMBOL_NAME(sys_brk)			/* 45 */
	.long SYMBOL_NAME(sys_setgid16)
	.long SYMBOL_NAME(sys_getgid16)
	.long SYMBOL_NAME(sys_signal)
	.long SYMBOL_NAME(sys_geteuid16)
	.long SYMBOL_NAME(sys_getegid16)	/* 50 */
	.long SYMBOL_NAME(sys_acct)
	.long SYMBOL_NAME(sys_umount)					/* recycled never used phys() */
	.long SYMBOL_NAME(sys_ni_syscall)				/* old lock syscall holder */
	.long SYMBOL_NAME(sys_ioctl)
	.long SYMBOL_NAME(sys_fcntl)		/* 55 */
	.long SYMBOL_NAME(sys_ni_syscall)				/* old mpx syscall holder */
	.long SYMBOL_NAME(sys_setpgid)
	.long SYMBOL_NAME(sys_ni_syscall)				/* old ulimit syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_umask)		/* 60 */
	.long SYMBOL_NAME(sys_chroot)
	.long SYMBOL_NAME(sys_ustat)
	.long SYMBOL_NAME(sys_dup2)
	.long SYMBOL_NAME(sys_getppid)
	.long SYMBOL_NAME(sys_getpgrp)		/* 65 */
	.long SYMBOL_NAME(sys_setsid)
	.long SYMBOL_NAME(sys_sigaction)
	.long SYMBOL_NAME(sys_sgetmask)
	.long SYMBOL_NAME(sys_ssetmask)
	.long SYMBOL_NAME(sys_setreuid16)	/* 70 */
	.long SYMBOL_NAME(sys_setregid16)
	.long SYMBOL_NAME(sys_sigsuspend)
	.long SYMBOL_NAME(sys_sigpending)
	.long SYMBOL_NAME(sys_sethostname)
	.long SYMBOL_NAME(sys_setrlimit)	/* 75 */
	.long SYMBOL_NAME(sys_old_getrlimit)
	.long SYMBOL_NAME(sys_getrusage)
	.long SYMBOL_NAME(sys_gettimeofday)
	.long SYMBOL_NAME(sys_settimeofday)
	.long SYMBOL_NAME(sys_getgroups16)	/* 80 */
	.long SYMBOL_NAME(sys_setgroups16)
	.long SYMBOL_NAME(old_select)
	.long SYMBOL_NAME(sys_symlink)
	.long SYMBOL_NAME(sys_lstat)
	.long SYMBOL_NAME(sys_readlink)		/* 85 */
	.long SYMBOL_NAME(sys_uselib)
	.long SYMBOL_NAME(sys_swapon)
	.long SYMBOL_NAME(sys_reboot)
	.long SYMBOL_NAME(old_readdir)
	.long SYMBOL_NAME(old_mmap)			/* 90 */
	.long SYMBOL_NAME(sys_munmap)
	.long SYMBOL_NAME(sys_truncate)
	.long SYMBOL_NAME(sys_ftruncate)
	.long SYMBOL_NAME(sys_fchmod)
	.long SYMBOL_NAME(sys_fchown16)		/* 95 */
	.long SYMBOL_NAME(sys_getpriority)
	.long SYMBOL_NAME(sys_setpriority)
	.long SYMBOL_NAME(sys_ni_syscall)				/* old profil syscall holder */
	.long SYMBOL_NAME(sys_statfs)
	.long SYMBOL_NAME(sys_fstatfs)		/* 100 */
	.long SYMBOL_NAME(sys_ioperm)
	.long SYMBOL_NAME(sys_socketcall)
	.long SYMBOL_NAME(sys_syslog)
	.long SYMBOL_NAME(sys_setitimer)
	.long SYMBOL_NAME(sys_getitimer)	/* 105 */
	.long SYMBOL_NAME(sys_newstat)
	.long SYMBOL_NAME(sys_newlstat)
	.long SYMBOL_NAME(sys_newfstat)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_ni_syscall)	/* iopl for i386 */ /* 110 */
	.long SYMBOL_NAME(sys_vhangup)
	.long SYMBOL_NAME(sys_ni_syscall)	/* obsolete idle() syscall */
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86old for i386 */
	.long SYMBOL_NAME(sys_wait4)
	.long SYMBOL_NAME(sys_swapoff)		/* 115 */
	.long SYMBOL_NAME(sys_sysinfo)
	.long SYMBOL_NAME(sys_ipc)
	.long SYMBOL_NAME(sys_fsync)
	.long SYMBOL_NAME(sys_sigreturn)
	.long SYMBOL_NAME(sys_clone)		/* 120 */
	.long SYMBOL_NAME(sys_setdomainname)
	.long SYMBOL_NAME(sys_newuname)
	.long SYMBOL_NAME(sys_cacheflush)	/* modify_ldt for i386 */
	.long SYMBOL_NAME(sys_adjtimex)
	.long SYMBOL_NAME(sys_mprotect)		/* 125 */
	.long SYMBOL_NAME(sys_sigprocmask)
	.long SYMBOL_NAME(sys_create_module)
	.long SYMBOL_NAME(sys_init_module)
	.long SYMBOL_NAME(sys_delete_module)
	.long SYMBOL_NAME(sys_get_kernel_syms)	/* 130 */
	.long SYMBOL_NAME(sys_quotactl)
	.long SYMBOL_NAME(sys_getpgid)
	.long SYMBOL_NAME(sys_fchdir)
	.long SYMBOL_NAME(sys_bdflush)
	.long SYMBOL_NAME(sys_sysfs)		/* 135 */
	.long SYMBOL_NAME(sys_personality)
	.long SYMBOL_NAME(sys_ni_syscall)	/* for afs_syscall */
	.long SYMBOL_NAME(sys_setfsuid16)
	.long SYMBOL_NAME(sys_setfsgid16)
	.long SYMBOL_NAME(sys_llseek)		/* 140 */
	.long SYMBOL_NAME(sys_getdents)
	.long SYMBOL_NAME(sys_select)
	.long SYMBOL_NAME(sys_flock)
	.long SYMBOL_NAME(sys_msync)
	.long SYMBOL_NAME(sys_readv)		/* 145 */
	.long SYMBOL_NAME(sys_writev)
	.long SYMBOL_NAME(sys_getsid)
	.long SYMBOL_NAME(sys_fdatasync)
	.long SYMBOL_NAME(sys_sysctl)
	.long SYMBOL_NAME(sys_mlock)		/* 150 */
	.long SYMBOL_NAME(sys_munlock)
	.long SYMBOL_NAME(sys_mlockall)
	.long SYMBOL_NAME(sys_munlockall)
	.long SYMBOL_NAME(sys_sched_setparam)
	.long SYMBOL_NAME(sys_sched_getparam)   /* 155 */
	.long SYMBOL_NAME(sys_sched_setscheduler)
	.long SYMBOL_NAME(sys_sched_getscheduler)
	.long SYMBOL_NAME(sys_sched_yield)
	.long SYMBOL_NAME(sys_sched_get_priority_max)
	.long SYMBOL_NAME(sys_sched_get_priority_min)  /* 160 */
	.long SYMBOL_NAME(sys_sched_rr_get_interval)
	.long SYMBOL_NAME(sys_nanosleep)
	.long SYMBOL_NAME(sys_mremap)
	.long SYMBOL_NAME(sys_setresuid16)
	.long SYMBOL_NAME(sys_getresuid16)	/* 165 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* for vm86 */
	.long SYMBOL_NAME(sys_query_module)
	.long SYMBOL_NAME(sys_poll)
	.long SYMBOL_NAME(sys_nfsservctl)
	.long SYMBOL_NAME(sys_setresgid16)	/* 170 */
	.long SYMBOL_NAME(sys_getresgid16)
	.long SYMBOL_NAME(sys_prctl)
	.long SYMBOL_NAME(sys_rt_sigreturn)
	.long SYMBOL_NAME(sys_rt_sigaction)
	.long SYMBOL_NAME(sys_rt_sigprocmask)	/* 175 */
	.long SYMBOL_NAME(sys_rt_sigpending)
	.long SYMBOL_NAME(sys_rt_sigtimedwait)
	.long SYMBOL_NAME(sys_rt_sigqueueinfo)
	.long SYMBOL_NAME(sys_rt_sigsuspend)
	.long SYMBOL_NAME(sys_pread)		/* 180 */
	.long SYMBOL_NAME(sys_pwrite)
	.long SYMBOL_NAME(sys_lchown16);
	.long SYMBOL_NAME(sys_getcwd)
	.long SYMBOL_NAME(sys_capget)
	.long SYMBOL_NAME(sys_capset)           /* 185 */
	.long SYMBOL_NAME(sys_sigaltstack)
	.long SYMBOL_NAME(sys_sendfile)
	.long SYMBOL_NAME(sys_ni_syscall)		/* streams1 */
	.long SYMBOL_NAME(sys_ni_syscall)		/* streams2 */
	.long SYMBOL_NAME(sys_vfork)            /* 190 */
	.long SYMBOL_NAME(sys_getrlimit)
	.long SYMBOL_NAME(sys_mmap2)
	.long SYMBOL_NAME(sys_truncate64)
	.long SYMBOL_NAME(sys_ftruncate64)
	.long SYMBOL_NAME(sys_stat64)		/* 195 */
	.long SYMBOL_NAME(sys_lstat64)
	.long SYMBOL_NAME(sys_fstat64)
	.long SYMBOL_NAME(sys_chown)
	.long SYMBOL_NAME(sys_getuid)
	.long SYMBOL_NAME(sys_getgid)		/* 200 */
	.long SYMBOL_NAME(sys_geteuid)
	.long SYMBOL_NAME(sys_getegid)
	.long SYMBOL_NAME(sys_setreuid)
	.long SYMBOL_NAME(sys_setregid)
	.long SYMBOL_NAME(sys_getgroups)	/* 205 */
	.long SYMBOL_NAME(sys_setgroups)
	.long SYMBOL_NAME(sys_fchown)
	.long SYMBOL_NAME(sys_setresuid)
	.long SYMBOL_NAME(sys_getresuid)
	.long SYMBOL_NAME(sys_setresgid)	/* 210 */
	.long SYMBOL_NAME(sys_getresgid)
	.long SYMBOL_NAME(sys_lchown)
	.long SYMBOL_NAME(sys_setuid)
	.long SYMBOL_NAME(sys_setgid)
	.long SYMBOL_NAME(sys_setfsuid)		/* 215 */
	.long SYMBOL_NAME(sys_setfsgid)

	.space (NR_syscalls-216)*4

