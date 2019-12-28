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

#include <linux/timex.h>

#include <asm/ps/interrupts.h>
#include <asm/ps/timer.h>

#define FREQ_NOM  338720  /* Frequency ratio */
#define FREQ_CNT  33872
#define FREQ_REL  10

extern rwlock_t xtime_lock;

static inline int timer_intr_valid(void) 
{
	static unsigned long long ticks = 0;

	if (++ticks >= FREQ_REL) {
      ticks = 0;
		return 1;
	}
	return 0;
}

/*
 * Call the "do_timer()" routine every clocktick
 */
static void inline
timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	if (timer_intr_valid()) {
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
}

static void __init timer_enable(void)
{
   int reg;

   /* Reset timer */
   reg = 0;
   outw (reg, 0x1124);

   /* Set count limit */
   reg = FREQ_CNT;
   outw (reg, 0x1128);
   
   /* Run timer */
   reg = inw (0x1124);
   reg &= ~((TIMER_STOP | TIMER_CLOCK | TIMER_DIVSC) & 0x3ff);
   reg |= (TIMER_TO_TARGET | TIMER_INT) & 0x3ff;
   outw (reg, 0x1124);
}

struct irqaction irq0 = {timer_interrupt, SA_INTERRUPT, 0,
			 "timer", NULL, NULL};

extern int setup_ps_irq(int, struct irqaction *);

void __init time_init(void)
{
	unsigned int year, mon, day, hour, min, sec;
   
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

   timer_enable ();
   setup_ps_irq (TIMER2, &irq0);
}

void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;

	read_lock_irqsave(&xtime_lock, flags);
   *tv = xtime;
	read_unlock_irqrestore(&xtime_lock, flags);
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq(&xtime_lock);

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;

	write_unlock_irq(&xtime_lock);
}
