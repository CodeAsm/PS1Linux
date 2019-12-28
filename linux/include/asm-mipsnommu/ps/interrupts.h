/*  
 * Miscellaneous definitions used to initialise the interrupt vector table
 * with the machine-specific interrupt routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997 by Paul M. Antoine.
 * reworked 1998 by Harald Koerfgen.
 */

#ifndef __ASM_PS_INTERRUPTS_H 
#define __ASM_PS_INTERRUPTS_H 

/*
 * PlayStation Interrupts
 */
#include <asm/addrspace.h>

/*
 * Interrupt types list by priority
 */
#define VBL          7
#define GPU 	      1
#define CDROM	      2
#define DMA		      3
#define TIMER0		   4
#define TIMER1		   5
#define TIMER2		   6
#define CONTROLLER	0
#define SIO		      8
#define SPU		      9
#define PIO		      10

/*
 * Interrupt masks list
 */
#define VBL_MASK        0x001
#define GPU_MASK 	      0x002
#define CDROM_MASK	   0x004
#define DMA_MASK		   0x008
#define TIMER0_MASK		0x010
#define TIMER1_MASK		0x020
#define TIMER2_MASK		0x040
#define CONTROLLER_MASK	0x080
#define SIO_MASK		   0x100
#define SPU_MASK		   0x200
#define PIO_MASK	      0x400

#define NR_INTS	11

/*
 * Interrupt registers
 */
#define INT_ACKN_REG 0x1f801070
#define INT_MASK_REG 0x1f801074

#ifndef __ASSEMBLY__

/*
 * Interrupt table structure to hide differences between different
 * systems such.
 */
extern long cpu_mask_tbl[NR_INTS];
extern long cpu_irq_nr[NR_INTS];

#endif
#endif 
