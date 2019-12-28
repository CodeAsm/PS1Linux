/*
 * arch/mips/dec/int-handler.S
 *
 * Copyright (C) 1995, 1996, 1997 Paul M. Antoine and Harald Koerfgen
 * Copyright (C) 2000  Maciej W. Rozycki
 *
 * Written by Ralf Baechle and Andreas Busse, modified for DECStation
 * support by Paul Antoine and Harald Koerfgen.
 *
 * completly rewritten:
 * Copyright (C) 1998 Harald Koerfgen
 *
 */
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/addrspace.h>

#include <asm/ps/interrupts.h>


		.text
		.set	noreorder
/*
 * playstation_handle_int: Interrupt handler for PlayStation
 */
		.align	5
		NESTED(playstation_handle_int, PT_SIZE, ra)
		.set	noat
		SAVE_ALL
		CLI				# TEST: interrupts should be off
		.set	at
		.set	noreorder

		/*
		 * Get pending Interrupts
		 */
		lw	t0, INT_ACKN_REG
		lw	t2, INT_MASK_REG
		la	t1,cpu_mask_tbl
		and t0, t2     # isolate allowed ones


		/*
		 * Find irq with highest priority
		 */
      li    t4, NR_INTS
1:	   beq   t4, zero, ps_intr_unimplemented	
      subu  t4, t4, 1
      lw	   t2, (t1)
      nop
		move	t3,t0
      nop
		and	t3,t2
		beq	t3,zero,1b
		addu	t1,PTRSIZE		# delay slot

		/*
		 * Do the low-level stuff
		 */
		lw	a0, %lo(cpu_irq_nr-cpu_mask_tbl-PTRSIZE)(t1)
      nop

		/* Acknowledge interrupt here */
//		not t2
//		sw t2, INT_ACKN_REG
	
		/*
		 * Handle it
		 */
      jal	do_IRQ
		 move	a1,sp
       
		j	ret_from_irq
		 nop
		
		END(playstation_handle_int)
      
/*
 * Generic unimplemented interrupt routines - ivec_tbl is initialised to
 * point all interrupts here.  The table is then filled in by machine-specific
 * initialisation in ps_setup().
 */
		EXPORT(ps_intr_unimplemented)
ps_intr_unimplemented:
		lw	a1, INT_ACKN_REG
		nop				# to be sure...
      and a1, a1, 0x7ff
		PANIC("Unimplemented cpu interrupt 0x%x !");


/*
 * FIXME: This interrupt vector table is experimental.  It is initialised with 
 *	  *_intr_unimplemented and filled in with the addresses of
 *	  machine-specific interrupt routines in ps_setup().
 *
 *	  The mask_tbls contain the interrupt masks which are used. It is 
 *	  initialised with all possible interrupt status bits set, so that 
 * 	  unused Interrupts are catched.
 */
		.data
      .align PTRLOG
		EXPORT(cpu_mask_tbl)
cpu_mask_tbl:
      .space (NR_INTS)*PTRSIZE
		.word	0x0000ffff		# End of list

		EXPORT(cpu_irq_nr)
cpu_irq_nr:
      .space (NR_INTS)*PTRSIZE
		.word	NR_INTS		   # End of list
