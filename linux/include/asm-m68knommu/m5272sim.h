/****************************************************************************/

/*
 *	m5272sim.h -- ColdFire 5272 System Integration Module support.
 *
 *	(C) Copyright 1999, Greg Ungerer (gerg@lineo.com)
 * 	(C) Copyright 2000, Lineo Inc. (www.lineo.com) 
 */

/****************************************************************************/
#ifndef	m5272sim_h
#define	m5272sim_h
/****************************************************************************/

#include <linux/config.h>

/*
 *	Define the 5272 SIM register set addresses.
 */
#define	MCFSIM_SCR		0x04		/* SIM Config reg (r/w) */
#define	MCFSIM_SPR		0x06		/* System Protection reg (r/w)*/
#define	MCFSIM_PMR		0x08		/* Power Management reg (r/w) */
#define	MCFSIM_APMR		0x0e		/* Active Low Power reg (r/w) */
#define	MCFSIM_DIR		0x10		/* Device Identity reg (r/w) */

#define	MCFSIM_ICR1		0x20		/* Intr Ctrl reg 1 (r/w) */
#define	MCFSIM_ICR2		0x24		/* Intr Ctrl reg 2 (r/w) */
#define	MCFSIM_ICR3		0x28		/* Intr Ctrl reg 3 (r/w) */
#define	MCFSIM_ICR4		0x2c		/* Intr Ctrl reg 4 (r/w) */

#define MCFSIM_ISR		0x30		/* Interrupt Source reg (r/w) */
#define MCFSIM_PITR		0x34		/* Interrupt Transition (r/w) */
#define	MCFSIM_PIWR		0x38		/* Interrupt Wakeup reg (r/w) */
#define	MCFSIM_PIVR		0x3f		/* Interrupt Vector reg (r/w( */

#if 0
#define	MCFSIM_DCRR		0x46		/* DRAM Refresh reg (r/w) */
#define	MCFSIM_DCTR		0x4a		/* DRAM Timing reg (r/w) */
#define	MCFSIM_DCAR0		0x4c		/* DRAM 0 Address reg(r/w) */
#define	MCFSIM_DCMR0		0x50		/* DRAM 0 Mask reg (r/w) */
#define	MCFSIM_DCCR0		0x57		/* DRAM 0 Control reg (r/w) */
#define	MCFSIM_DCAR1		0x58		/* DRAM 1 Address reg (r/w) */
#define	MCFSIM_DCMR1		0x5c		/* DRAM 1 Mask reg (r/w) */
#define	MCFSIM_DCCR1		0x63		/* DRAM 1 Control reg (r/w) */
#endif

#if 0
#define	MCFSIM_CSAR0		0x64		/* CS 0 Address 0 reg (r/w) */
#define	MCFSIM_CSMR0		0x68		/* CS 0 Mask 0 reg (r/w) */
#define	MCFSIM_CSCR0		0x6e		/* CS 0 Control reg (r/w) */
#define	MCFSIM_CSAR1		0x70		/* CS 1 Address reg (r/w) */
#define	MCFSIM_CSMR1		0x74		/* CS 1 Mask reg (r/w) */
#define	MCFSIM_CSCR1		0x7a		/* CS 1 Control reg (r/w) */
#define	MCFSIM_CSAR2		0x7c		/* CS 2 Address reg (r/w) */
#define	MCFSIM_CSMR2		0x80		/* CS 2 Mask reg (r/w) */
#define	MCFSIM_CSCR2		0x86		/* CS 2 Control reg (r/w) */
#define	MCFSIM_CSAR3		0x88		/* CS 3 Address reg (r/w) */
#define	MCFSIM_CSMR3		0x8c		/* CS 3 Mask reg (r/w) */
#define	MCFSIM_CSCR3		0x92		/* CS 3 Control reg (r/w) */
#define	MCFSIM_CSAR4		0x94		/* CS 4 Address reg (r/w) */
#define	MCFSIM_CSMR4		0x98		/* CS 4 Mask reg (r/w) */
#define	MCFSIM_CSCR4		0x9e		/* CS 4 Control reg (r/w) */
#define	MCFSIM_CSAR5		0xa0		/* CS 5 Address reg (r/w) */
#define	MCFSIM_CSMR5		0xa4		/* CS 5 Mask reg (r/w) */
#define	MCFSIM_CSCR5		0xaa		/* CS 5 Control reg (r/w) */
#define	MCFSIM_CSAR6		0xac		/* CS 6 Address reg (r/w) */
#define	MCFSIM_CSMR6		0xb0		/* CS 6 Mask reg (r/w) */
#define	MCFSIM_CSCR6		0xb6		/* CS 6 Control reg (r/w) */
#define	MCFSIM_CSAR7		0xb8		/* CS 7 Address reg (r/w) */
#define	MCFSIM_CSMR7		0xbc		/* CS 7 Mask reg (r/w) */
#define	MCFSIM_CSCR7		0xc2		/* CS 7 Control reg (r/w) */
#define	MCFSIM_DMCR		0xc6		/* Default control */
#endif

#if 0
#define	MCFSIM_PAR		0xca		/* Pin Assignment reg (r/w) */
#define	MCFSIM_PADDR		0x1c5		/* Parallel Direction (r/w) */
#define	MCFSIM_PADAT		0x1c9		/* Parallel Port Value (r/w) */
#endif

#if 0
/*
 *	Some symbol defines for the Parallel Port Pin Assignment Register
 */
#define MCFSIM_PAR_DREQ0        0x100           /* Set to select DREQ0 input */
                                                /* Clear to select T0 input */
#define MCFSIM_PAR_DREQ1        0x200           /* Select DREQ1 input */
                                                /* Clear to select T0 output */
#endif

/****************************************************************************/
#endif	/* m5272sim_h */
