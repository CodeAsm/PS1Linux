/* $Id: cache.h,v 1.1.1.1 2001/02/22 14:58:57 serg Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1998, 1999 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_CACHE_H
#define _ASM_CACHE_H

#include <linux/config.h>

#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_R6000)
#define L1_CACHE_BYTES		16
#else
#define L1_CACHE_BYTES 		32	/* A guess */
#endif

#define SMP_CACHE_BYTES		L1_CACHE_BYTES

#endif /* _ASM_CACHE_H */
