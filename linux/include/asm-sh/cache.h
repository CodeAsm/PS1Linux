/* $Id: cache.h,v 1.1.1.1 2001/02/22 14:58:58 serg Exp $
 *
 * include/asm-sh/cache.h
 *
 * Copyright 1999 (C) Niibe Yutaka
 */
#ifndef __ASM_SH_CACHE_H
#define __ASM_SH_CACHE_H

/* bytes per L1 cache line */
#if defined(__sh3__)
#define        L1_CACHE_BYTES  16
#elif defined(__SH4__)
#define        L1_CACHE_BYTES  32
#endif

#endif /* __ASM_SH_CACHE_H */
