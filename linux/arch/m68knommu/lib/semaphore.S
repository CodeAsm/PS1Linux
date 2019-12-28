/*
 *  linux/arch/m68k/lib/semaphore.S
 *
 *  Copyright (C) 1996  Linus Torvalds
 *
 *  m68k version by Andreas Schwab
 *
 *  MAR/1999 -- modified to support ColdFire (gerg@lineo.com)
 */

#include <linux/linkage.h>
#include <linux/config.h>
#include <asm/semaphore.h>

/*
 * "down_failed" is called with the eventual return address
 * in %a0, and the address of the semaphore in %a1. We need
 * to increment the number of waiters on the semaphore,
 * call "__down()", and then eventually return to try again.
 */
ENTRY(__down_failed)
#ifdef CONFIG_COLDFIRE
	subl #12,%sp
	moveml %a0/%d0/%d1,(%sp)
#else
	moveml %a0/%d0/%d1,-(%sp)
#endif
	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(__down)
	movel (%sp)+,%a1
	movel (%sp)+,%d0
	movel (%sp)+,%d1
	rts

ENTRY(__down_failed_interruptible)
	movel %a0,-(%sp)
	movel %d1,-(%sp)
	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(__down_interruptible)
	movel (%sp)+,%a1
	movel (%sp)+,%d1
	rts

ENTRY(__up_wakeup)
#ifdef CONFIG_COLDFIRE
	subl #12,%sp
	moveml %a0/%d0/%d1,(%sp)
#else
	moveml %a0/%d0/%d1,-(%sp)
#endif
	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(__up)
	movel (%sp)+,%a1
	movel (%sp)+,%d0
	movel (%sp)+,%d1
	rts

ENTRY(__down_failed_trylock)
	movel %a0,-(%sp)
	movel %d1,-(%sp)
	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(__down_trylock)
	movel (%sp)+,%a1
	movel (%sp)+,%d1
	movel (%sp)+,%a0
	rts

ENTRY(__down_read_failed)
#ifdef CONFIG_COLDFIRE
	subl #12,%sp
	moveml %a0/%d0/%d1,(%sp)
#else
	moveml %a0/%d0/%d1,-(%sp)
#endif
	jcc 3f
1:	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(down_read_failed_biased)
	movel (%sp)+,%a1
#ifdef CONFIG_COLDFIRE
2:	moveml (%sp),%a0/%d0/%d1
    addl #12,%sp
#else
2:	moveml (%sp)+,%a0/%d0/%d1
#endif
	rts

3:	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(down_read_failed)
	movel (%sp)+,%a1
	subql #1,%a1@
	jpl 2b
	jcc 3b
	jra 1b

ENTRY(__down_write_failed)
#ifdef CONFIG_COLDFIRE
	subl #12,%sp
	moveml %a0/%d0/%d1,(%sp)
#else
	moveml %a0/%d0/%d1,-(%sp)
#endif
	jcc 3f
1:	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(down_write_failed_biased)
	movel (%sp)+,%a1
#ifdef CONFIG_COLDFIRE
2:	moveml (%sp),%a0/%d0/%d1
    addl #12,%sp
#else
2:	moveml (%sp)+,%a0/%d0/%d1
#endif
	rts

3:	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(down_write_failed)
	movel (%sp)+,%a1
#ifdef CONFIG_COLDFIRE
	movel #RW_LOCK_BIAS,%d0 /* DAVIDM not sure here */
	subl %d0,%a1@
#else
	subl #RW_LOCK_BIAS,%a1@
#endif
	jpl 2b
	jcc 3b
	jra 1b

#if 0
ENTRY(__rwsem_wake)
#ifdef CONFIG_COLDFIRE
	subl #12,%sp
	moveml %a0/%d0/%d1,(%sp)
#else
	moveml %a0/%d0/%d1,-(%sp)
#endif
	jeq 1f
	movel %a1,-(%sp)
	jbsr SYMBOL_NAME(rwsem_wake_readers)
	jra 2f
1:	movel %a1,-(%sp)
	jbsr rwsem_wake_writer
2:	movel (%sp)+,%a1
#ifdef CONFIG_COLDFIRE
	moveml (%sp),%a0/%d0/%d1
    addl #12,%sp
#else
	moveml (%sp)+,%a0/%d0/%d1
#endif
	rts

#endif
