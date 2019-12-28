/*
 *  linux/arch/mips/dec/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Copyright (C) 2000  Maciej W. Rozycki
 *
 * This file contains the time handling details for PC-style clocks as
 * found in some MIPS systems.
 *
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/mipsregs.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/mc146818rtc.h>
#include <linux/timex.h>

#include <asm/div64.h>

extern volatile unsigned long wall_jiffies;
extern rwlock_t xtime_lock;

/*
 * Change this if you have some constant time drift
 */
/* This is the value for the PC-style PICs. */
/* #define USECS_PER_JIFFY (1000020/HZ) */

/* This is for machines which generate the exact clock. */
#define USECS_PER_JIFFY (1000000/HZ)
#define USECS_PER_JIFFY_FRAC (0x100000000*1000000/HZ&0xffffffff)

/* Cycle counter value at the previous timer interrupt.. */

static unsigned int timerhi, timerlo;

/*
 * Cached "1/(clocks per usec)*2^32" value.
 * It has to be recalculated once each jiffy.
 */
static unsigned long cached_quotient = 0;

/* Last jiffy when do_fast_gettimeoffset() was called. */
static unsigned long last_jiffies = 0;

#define TICK_SIZE tick

static unsigned long do_slow_gettimeoffset(void)
{
	/*
	 * This is a kludge
	 */
	return 0;
}

static unsigned long (*do_gettimeoffset) (void) = do_slow_gettimeoffset;

/*
 * This version of gettimeofday has near microsecond resolution.
 */
void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;

	read_lock_irqsave(&xtime_lock, flags);
	*tv = xtime;
	tv->tv_usec += do_gettimeoffset();

	/*
	 * xtime is atomically updated in timer_bh. jiffies - wall_jiffies
	 * is nonzero if the timer bottom half hasnt executed yet.
	 */
	if (jiffies - wall_jiffies)
		tv->tv_usec += USECS_PER_JIFFY;

	read_unlock_irqrestore(&xtime_lock, flags);

	if (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
	}
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq(&xtime_lock);

	/* This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */
	tv->tv_usec -= do_gettimeoffset();

	if (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;

	write_unlock_irq(&xtime_lock);
}

/*
 * Call the "do_timer()" routine every clocktick
 */
static void inline
timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	if (!user_mode(regs)) {
		if (prof_buffer && current->pid) {
			extern int _stext;
			unsigned long pc = regs->cp0_epc;

			pc -= (unsigned long) &_stext;
			pc >>= prof_shift;
			/*
			 * Dont ignore out-of-bounds pc values silently,
			 * put them into the last histogram slot, so if
			 * present, they will show up as a sharp peak.
			 */
			if (pc > prof_len - 1)
				pc = prof_len - 1;
			atomic_inc((atomic_t *) & prof_buffer[pc]);
		}
	}
	do_timer(regs);
}

struct irqaction irq0 = {timer_interrupt, SA_INTERRUPT, 0,
			 "timer", NULL, NULL};

void (*board_time_init) (struct irqaction * irq);

void __init time_init(void)
{
	unsigned int year, mon, day, hour, min, sec, real_year;
	int i;
   
	sec = 0;
	min = 0;
	hour = 0;
	day = 31;
	mon = 10;
	year = 2000;

	write_lock_irq(&xtime_lock);
	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_usec = 0;
	write_unlock_irq(&xtime_lock);

	board_time_init(&irq0);
}
