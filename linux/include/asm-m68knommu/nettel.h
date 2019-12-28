/****************************************************************************/
/*
 *	nettel.h -- Lineo (formerly Moreton Bay) NETtel support.
 *
 *	(C) Copyright 1999-2000, Moreton Bay (www.moretonbay.com)
 * 	(C) Copyright 2000, Lineo Inc. (www.lineo.com) 
 */
/****************************************************************************/
#ifndef	nettel_h
#define	nettel_h

#include <linux/config.h>

/****************************************************************************/
#ifdef CONFIG_NETtel
/****************************************************************************/

#ifdef CONFIG_COLDFIRE
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#endif

#define       NETtel_DCD1             0x0001
#define       NETtel_DCD0             0x0002
#define       NETtel_DTR1             0x0004
#define       NETtel_DTR0             0x0008

#if defined(CONFIG_M5307)
#define       NETtel_LEDADDR          0x30400000
#elif defined(CONFIG_M5206e)
#define       NETtel_LEDADDR          0x50000000
#endif

#ifndef __ASSEMBLY__
#ifdef CONFIG_M5307
extern volatile unsigned short ppdata;
#endif /* CONFIG_M5307 */
#endif /* __ASSEMBLY__ */

/****************************************************************************/
#endif /* CONFIG_NETtel */
/****************************************************************************/
#endif	/* nettel_h */
/****************************************************************************/
