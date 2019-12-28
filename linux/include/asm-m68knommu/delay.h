#ifndef _M68KNOMMU_DELAY_H
#define _M68KNOMMU_DELAY_H

/*
 * Copyright (C) 1994 Hamish Macdonald
 *
 * Delay routines, using a pre-computed "loops_per_second" value.
 */

extern __inline__ void __delay(unsigned long loops)
{
	__asm__ __volatile__ ("1: subql #1,%0; jcc 1b"
		: "=d" (loops) : "0" (loops));
}

/*
 * Use only for very small delays ( < 1 msec).  Should probably use a
 * lookup table, really, as the multiplications take much too long with
 * short delays.  This is a "reasonable" implementation, though (and the
 * first constant multiplications gets optimized away if the delay is
 * a constant)  
 */
extern __inline__ void udelay(unsigned long usecs)
{
#ifdef CONFIG_M68332
        usecs *= 0x000010c6;       
       __asm__ __volatile__ ("mulul %1,%0:%2"
                    : "=d" (usecs)
                  : "d" (usecs),
                   "d" (loops_per_jiffy*HZ));
	__delay(usecs);

#elif defined(CONFIG_M68328) || defined(CONFIG_M68EZ328) || \
		defined(CONFIG_COLDFIRE)
	/* Sigh */
	__delay(usecs);
#else
	unsigned long tmp;

	usecs *= 4295;		/* 2**32 / 1000000 */
	__asm__ ("mulul %2,%0:%1"
		: "=d" (usecs), "=d" (tmp)
		: "d" (usecs), "1" (loops_per_jiffy*HZ));
	__delay(usecs);
#endif
}

#if 0 /* DAVIDM - elsewhere under m68knommu */
extern __inline__ unsigned long muldiv(unsigned long a, unsigned long b, unsigned long c)
{
	unsigned long tmp;

	__asm__ ("mulul %2,%0:%1; divul %3,%0:%1"
		: "=d" (tmp), "=d" (a)
		: "d" (b), "d" (c), "1" (a));
	return a;
}
#endif

#endif /* defined(_M68KNOMMU_DELAY_H) */
