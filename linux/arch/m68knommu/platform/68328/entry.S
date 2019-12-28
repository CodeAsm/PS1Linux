/* -*- mode: asm -*-
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
 */

/*
 * entry.S  contains the system-call and fault low-level handling routines.
 * This also contains the timer-interrupt handler, as well as all interrupts
 * and faults that can result in a task-switch.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 */

/*
 * 12/03/96 Jes: Currently we only support m68k single-cpu systems, so
 *               all pointers that used to be 'current' are now entry
 *               number 0 in the 'current_set' list.
 */

#include <linux/sys.h>
#include <linux/config.h>
#include <linux/linkage.h>
#include "entry.h" /* FIXME */
#include <asm/errno.h>
#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/traps.h>

#include "m68k_defs.h"

.globl SYMBOL_NAME(system_call), SYMBOL_NAME(buserr), SYMBOL_NAME(trap)
.globl SYMBOL_NAME(resume), SYMBOL_NAME(ret_from_exception)
.globl SYMBOL_NAME(ret_from_signal)
.globl SYMBOL_NAME(sys_call_table)
.globl SYMBOL_NAME(sys_fork), SYMBOL_NAME(sys_clone), SYMBOL_NAME(sys_vfork)
.globl SYMBOL_NAME(ret_from_interrupt), SYMBOL_NAME(bad_interrupt)
.globl SYMBOL_NAME(inthandler1), SYMBOL_NAME(inthandler2),SYMBOL_NAME(inthandler3)
.globl SYMBOL_NAME(inthandler4),SYMBOL_NAME(inthandler5),SYMBOL_NAME(inthandler6)
.globl SYMBOL_NAME(inthandler7)

.text
ENTRY(buserr)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	movel	%sp,%sp@- 		/* stack frame pointer argument*/
	bsrw	SYMBOL_NAME(buserr_c)
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)

ENTRY(trap)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	movel	%sp,%sp@- 		/* stack frame pointer argument*/
	bsrw	SYMBOL_NAME(trap_c)
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)

ENTRY(reschedule)
	/* save top of frame*/
	movel	%sp,%curptr@(TASK_THREAD+THREAD_ESP0)

	pea	SYMBOL_NAME(ret_from_exception)
	jmp	SYMBOL_NAME(schedule)

	/* After a fork we jump here directly from resume,*/
	/* so that %d1 contains the previous task*/
	/* Theoretically only needed on SMP, but let's watch*/
	/* what happens in schedule_tail() in future...*/
ENTRY(ret_from_fork)
	movel	%d1,%sp@-
	jsr	SYMBOL_NAME(schedule_tail)
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)

badsys:
	movel	#-ENOSYS,%sp@(PT_D0)
	jra	SYMBOL_NAME(ret_from_exception)

do_trace:
	movel	#-ENOSYS,%sp@(PT_D0)	/* needed for strace*/
	subql	#4,%sp
	SAVE_SWITCH_STACK
	jbsr	SYMBOL_NAME(syscall_trace)
	RESTORE_SWITCH_STACK
	addql	#4,%sp
	movel	%sp@(PT_ORIG_D0),%d1
	movel	#-ENOSYS,%d0
	cmpl	#NR_syscalls,%d1
	jcc	1f
/*	jbsr	@(SYMBOL_NAME(sys_call_table),%d1:l:4)@(0) */
	lsl	#2,%d1
#if 1
	lea	SYMBOL_NAME(sys_call_table), %a0
	jbsr	%a0@(%d1)
#else
	bsrw	SYMBOL_NAME(sys_call_table)@(%d1)
#endif

1:	movel	%d0,%sp@(PT_D0)		/* save the return value*/
	subql	#4,%sp			/* dummy return address*/
	SAVE_SWITCH_STACK
	jbsr	SYMBOL_NAME(syscall_trace)

SYMBOL_NAME_LABEL(ret_from_signal)
	RESTORE_SWITCH_STACK
	addql	#4,%sp
	jra	SYMBOL_NAME(ret_from_exception)

ENTRY(system_call)
	SAVE_ALL_SYS

	GET_CURRENT(%d1)
	/* save top of frame*/
	movel	%sp,%curptr@(TASK_THREAD+THREAD_ESP0)

	btst	#PF_TRACESYS_BIT,%curptr@(TASK_FLAGS+PF_TRACESYS_OFF)
	jne	do_trace
	cmpl	#NR_syscalls,%d0
	jcc	badsys
/*	jbsr	@(SYMBOL_NAME(sys_call_table),%d0:l:4)@(0) */
	lsl	#2,%d0
#if 1
	lea	SYMBOL_NAME(sys_call_table),%a0
	jbsr	%a0@(%d0)
#else
	jbsr	SYMBOL_NAME(sys_call_table)@(%d0)
#endif
	movel	%d0,%sp@(PT_D0)		/* save the return value*/

SYMBOL_NAME_LABEL(ret_from_exception)
	btst	#5,%sp@(PT_SR)		/* check if returning to kernel*/
	bnes	2f			/* if so, skip resched, signals*/
	/* only allow interrupts when we are really the last one on the*/
	/* kernel stack, otherwise stack overflow can occur during*/
	/* heavy interupt load*/
	andw	#ALLOWINT,%sr
	tstl	%curptr@(TASK_NEEDRESCHED)
	jne	SYMBOL_NAME(reschedule)
#if 0
	cmpl	#SYMBOL_NAME(task),%curptr	/* task[0] cannot have signals*/
	jeq	2f
#endif
					/* check for delayed trace*/
	bclr	#PF_DTRACE_BIT,%curptr@(TASK_FLAGS+PF_DTRACE_OFF)
	jne	do_delayed_trace
5:
	tstl	%curptr@(TASK_STATE)	/* state*/
	jne	SYMBOL_NAME(reschedule)

	tstl	%curptr@(TASK_SIGPENDING)
	jne	Lsignal_return
2:	RESTORE_ALL

Lsignal_return:
	subql	#4,%sp			/* dummy return address*/
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	clrl	%sp@-
	bsrw	SYMBOL_NAME(do_signal)
	addql	#8,%sp
	RESTORE_SWITCH_STACK
	addql	#4,%sp
	RESTORE_ALL

do_delayed_trace:
	bclr	#7,%sp@(PT_SR)		/* clear trace bit in SR*/
	pea	1			/* send SIGTRAP*/
	movel	%curptr,%sp@-
	pea	LSIGTRAP
	jbsr	SYMBOL_NAME(send_sig)
	addql	#8,%sp
	addql	#4,%sp
	jra	5b
/*
** This is the main interrupt handler, responsible for calling process_int()
*/
SYMBOL_NAME_LABEL(inthandler1)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#65,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler2)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#66,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler3)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#67,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler4)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#68,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler5)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#69,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler6)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#70,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler7)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#71,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler8)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#72,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(timerhandler)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#0x40,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(serialhandler)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	#0x42,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler_wrap)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	%d0,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(inthandler)
	SAVE_ALL_INT
	GET_CURRENT(%d0)
	addql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
					/*  put exception # in d0*/
/*	bfextu %sp@(PT_VECTOR){#4,#10},%d0 */
	movew	%sp@(PT_VECTOR), %d0
	and	#0x3ff, %d0

	movel	%sp,%sp@-
	movel	%d0,%sp@- 		/*  put vector # on stack*/
	jbsr	SYMBOL_NAME(process_int)/*  process the IRQ*/
3:     	addql	#8,%sp			/*  pop parameters off stack*/
	bra	ret_from_interrupt

SYMBOL_NAME_LABEL(ret_from_interrupt)
	subql	#1,SYMBOL_NAME(irq_stat)+8	/* local_irq_count*/
	jeq	1f
2:
	RESTORE_ALL
1:
#if 1
/*	bfextu  %sp@(PT_SR){#5,#3},%d0 */	/* Check for nested interrupt.*/
	moveb	%sp@(PT_SR), %d0
	and	#7, %d0

#if MAX_NOINT_IPL > 0
	cmpiw	#MAX_NOINT_IPL,%d0
#endif
	jhi	2b
#endif
	/* check if we need to do software interrupts */

	movel	SYMBOL_NAME(irq_stat),%d0	/* softirq_active*/
	andl	SYMBOL_NAME(irq_stat)+4,%d0	/* softirq_mask*/
	jeq	SYMBOL_NAME(ret_from_exception)

	pea	SYMBOL_NAME(ret_from_exception)
	jra	SYMBOL_NAME(do_softirq)


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

ENTRY(sys_clone)
	SAVE_SWITCH_STACK
	pea	%sp@(SWITCH_STACK_SIZE)
	jbsr	SYMBOL_NAME(m68k_clone)
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
	movew	%sr,%a0@(TASK_THREAD+THREAD_SR)
#ifdef USE_SFC_DFC
	/* save fs (sfc,%dfc) (may be pointing to kernel memory) */
	movec	%sfc,%d0
	movew	%d0,%a0@(TASK_THREAD+THREAD_FS)

	/* save usp */
	/* it is better to use a movel here instead of a movew 8*) */
	movec	%usp,%d0
	movel	%d0,%a0@(TASK_THREAD+THREAD_USP)
#endif
	/* save non-scratch registers on stack */
	SAVE_SWITCH_STACK

	/* save current kernel stack pointer */
	movel	%sp,%a0@(TASK_THREAD+THREAD_KSP)

	/* Return previous task in %d1 */
	movel	%curptr,%d1

	/* switch to new task (a1 contains new task) */
	movel	%a1,%curptr

	/* restore the kernel stack pointer */
	movel	%a1@(TASK_THREAD+THREAD_KSP),%sp

	/* restore non-scratch registers */
	RESTORE_SWITCH_STACK

	/* restore user stack pointer */
	movel	%a1@(TASK_THREAD+THREAD_USP),%a0
	movel	%a0,%usp
#ifdef USE_SFC_DFC
	/* restore fs (sfc,%dfc) */
	movew	%a1@(TASK_THREAD+THREAD_FS),%a0
	movec	%a0,%sfc
	movec	%a0,%dfc
#endif
	/* restore status register */
	movew	%a1@(TASK_THREAD+THREAD_SR),%sr

	rts

.data
ALIGN
SYMBOL_NAME_LABEL(sys_call_table)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 0  -  old "setup()" system call*/
	.long SYMBOL_NAME(sys_exit)
	.long SYMBOL_NAME(sys_fork)
	.long SYMBOL_NAME(sys_read)
	.long SYMBOL_NAME(sys_write)
	.long SYMBOL_NAME(sys_open)		/* 5 */
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
	.long SYMBOL_NAME(sys_ni_syscall)	/* 35 */		/* old ftime syscall holder */
	.long SYMBOL_NAME(sys_sync)
	.long SYMBOL_NAME(sys_kill)
	.long SYMBOL_NAME(sys_rename)
	.long SYMBOL_NAME(sys_mkdir)
	.long SYMBOL_NAME(sys_rmdir)		/* 40 */
	.long SYMBOL_NAME(sys_dup)
	.long SYMBOL_NAME(sys_pipe)
	.long SYMBOL_NAME(sys_times)
	.long SYMBOL_NAME(sys_ni_syscall)				/* old prof syscall holder */
	.long SYMBOL_NAME(sys_brk)		/* 45 */
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
	.long SYMBOL_NAME(old_mmap)		/* 90 */
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
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_getdents64)	/* 220 */

	.rept NR_syscalls-(.-SYMBOL_NAME(sys_call_table))/4
		.long SYMBOL_NAME(sys_ni_syscall)
	.endr
