/****************************************************************************/

/*
 *	coldfire.h -- Motorola ColdFire CPU sepecific defines
 *
 *	(C) Copyright 2000, Lineo (www.lineo.com)
 *	(C) Copyright 1999-2000, Greg Ungerer (gerg@lineo.com)
 */

/****************************************************************************/
#ifndef	coldfire_h
#define	coldfire_h
/****************************************************************************/

#include <linux/config.h>

/*
 *	Define the processor support peripherals base address.
 *	This is generally setup by the boards start up code.
 */
#define	MCF_MBAR	0x10000000

/*
 *	Define master clock frequency.
 */
#if defined(CONFIG_M5204)
#define	MCF_CLK		25000000
#elif defined(CONFIG_M5206)
#define	MCF_CLK		25000000
#elif defined(CONFIG_M5206e)
#if defined(CONFIG_NETtel)
#define	MCF_CLK		40000000
#elif defined(CONFIG_CFV240)
#define MCF_CLK         40000000
#else
#define	MCF_CLK		54000000
#endif
#elif defined(CONFIG_M5272)
#define	MCF_CLK		66000000
#elif defined(CONFIG_M5307)
#define	MCF_CLK		45000000
#elif defined(CONFIG_M5407)
#define	MCF_CLK		50000000
#endif

/****************************************************************************/
#endif	/* coldfire_h */
