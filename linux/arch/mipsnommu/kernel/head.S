/*
 * arch/mips/kernel/head.S
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1995 - 1999 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Modified for DECStation and hence R3000 support by Paul M. Antoine
 * Further modifications by David S. Miller and Harald Koerfgen
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * Head.S contains the MIPS exception handler and startup code.
 *
 **************************************************************************
 *  9 Nov, 2000.
 *  Added Cache Error exception handler and SBDDP EJTAG debug exception.
 *
 *  Kevin Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 *  Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 **************************************************************************
 */
#include <linux/config.h>
#include <linux/threads.h>

#include <asm/asm.h>
#include <asm/cacheops.h>
#include <asm/current.h>
#include <asm/offset.h>
#include <asm/processor.h>
#include <asm/regdef.h>
#include <asm/cachectl.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/bootinfo.h>

	.text
	/*
	 * Reserved space for exception handlers.
	 * Necessary for machines which link their kernels at KUSEG.
	 * FIXME: Use the initcode feature to get rid of unused handler
	 * variants.
	 */
	.fill	0x280
/*	
 * This is space for the interrupt handlers.
 * After trap_init() they are located at virtual address KUSEG.
 *
 * These handlers much be written in a relocatable manner
 * because based upon the cpu type an arbitrary one of the
 * following pieces of code will be copied to the KUSEG
 * vector location.
 */

	/* Cache Error */
	LEAF(except_vec2_generic)
	.set	noat
	.set    mips0
	/*
	 * This is a very bad place to be.  Our cache error
	 * detection has triggered.  If we have write-back data
	 * in the cache, we may not be able to recover.  As a
	 * first-order desperate measure, turn off KUSEG cacheing.
	 */
	mfc0	k0,CP0_CONFIG
	li	k1,~CONF_CM_CMASK
	and	k0,k0,k1
	ori	k0,k0,CONF_CM_UNCACHED
	mtc0	k0,CP0_CONFIG
	/* Give it a few cycles to sink in... */
	nop
	nop
	nop

	j	cache_parity_error
	nop
	END(except_vec2_generic)

	/* General exception vector R4000 version. */
	NESTED(except_vec3_r4000, 0, sp)
	.set	noat
	mfc0	k1, CP0_CAUSE
	andi	k1, k1, 0x7c
	li	k0, 31<<2
	beq	k1, k0, handle_vced
	 li	k0, 14<<2
	beq	k1, k0, handle_vcei
	 la	k0, exception_handlers
	addu	k0, k0, k1
	lw	k0, (k0)
	nop
	jr	k0
	 nop

/*
 * Big shit, we now may have two dirty primary cache lines for the same
 * physical address.  We can savely invalidate the line pointed to by
 * c0_badvaddr because after return from this exception handler the load /
 * store will be re-executed.
 */
	.set    mips3
handle_vced:
	mfc0	k0, CP0_BADVADDR
 li k1, -4
 and k0, k1
	mtc0	zero, CP0_TAGLO
	cache	Index_Store_Tag_D,(k0)
	cache	Hit_Writeback_Inv_SD,(k0)
#ifdef CONFIG_PROC_FS
	lui	k0, %hi(vced_count)
	lw	k1, %lo(vced_count)(k0)
	addiu	k1, 1
	sw	k1, %lo(vced_count)(k0)
#endif
	eret

handle_vcei:
	mfc0	k0, CP0_BADVADDR
	cache	Hit_Writeback_Inv_SD,(k0)		# also cleans pi
#ifdef CONFIG_PROC_FS
	lui	k0, %hi(vcei_count)
	lw	k1, %lo(vcei_count)(k0)
	addiu	k1, 1
	sw	k1, %lo(vcei_count)(k0)
#endif
	eret

	END(except_vec3_r4000)
	.set	at

	/* General exception vector. */
	NESTED(except_vec3_generic, 0, sp)
	.set	noat
	.set	mips0
	mfc0	k1, CP0_CAUSE
	la	k0, exception_handlers
	andi	k1, k1, 0x7c
	addu	k0, k0, k1
	lw	k0, (k0)
	nop
	jr	k0
	 nop
	END(except_vec3_generic)
	.set	at

	/*
	 * Special interrupt vector for embedded MIPS.  This is a
	 * dedicated interrupt vector which reduces interrupt processing
	 * overhead.  The jump instruction will be inserted here at
	 * initialization time.  This handler may only be 8 bytes in size!
	 */
	NESTED(except_vec4, 0, sp)
1:	j	1b			/* Dummy, will be replaced */
	 nop
	END(except_vec4)

	/*
	 * SBDDP EJTAG debug exception handler.
	 * The EJTAG debug exception entry point is 0xbfc00480, which
	 * normally is in the boot PROM, so the boot PROM must do a
	 * unconditional jump to this vector.
	 */	
	NESTED(except_vec_ejtag_debug, 0, sp)
	PRINT("SDBBP EJTAG debug exception - not handled yet, hang!\n");
1:	j	1b
	 nop
	END(except_vec_ejtag_debug)

	
/*
 * Kernel entry point
 */
NESTED(kernel_entry, 16, sp)
	.set	noreorder
	/* The following two symbols are used for kernel profiling. */
	EXPORT(stext)
	EXPORT(_stext)

	/*
	 * Stack for kernel and init, current variable
	 */
	la	$28, init_task_union
	addiu	t0, $28, KERNEL_STACK_SIZE-32
	sw	t0, kernelsp
	subu	sp, t0, 4*SZREG

	/* The firmware/bootloader passes argc/argp/envp
	 * to us as arguments.  But clear bss first because
	 * the romvec and other important info is stored there
	 * by prom_init().
	 */
	la	t0, _edata
	sw	zero, (t0)
	la	t1, (_end - 4)
1:
	addiu	t0, 4
	bne	t0, t1, 1b
	 sw	zero, (t0)

	jal	init_arch
	 nop
	END(kernel_entry)

/*
 * This buffer is reserved for the use of the cache error handler.
 */
		.data
		EXPORT(cache_error_buffer)
		.fill	32*4,1,0

EXPORT(kernelsp)
		PTR	0
EXPORT(current_pgd)
		PTR	0
		.text

		.org	0x1000
EXPORT(swapper_pg_dir)

		.org	0x2000
EXPORT(empty_bad_page)

		.org	0x3000
EXPORT(empty_bad_page_table)

		.org	0x4000
EXPORT(invalid_pte_table)

		.org	0x5000
/* XXX This label is required to keep GAS trying to be too clever ...
   Bug?  */
dummy:
/*
 * Align to 8kb boundary for init_task_union which follows in the
 * .text segment.
 */
		.align	13
