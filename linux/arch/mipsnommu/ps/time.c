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

#include <asm/ps/interrupts.h>
#include <asm/ps/timer.h>

#define FREQ_NOM  338720  /* Frequency ratio */
#define FREQ_CNT  33872
#define FREQ_REL  10

/* This is for machines which generate the exact clock. */
#define USECS_PER_JIFFY (1000000/HZ)

extern volatile unsigned long wall_jiffies;
extern rwlock_t xtime_lock;

#define TICK_SIZE tick

/*
 * In order to set the CMOS clock precisely, set_rtc_mmss has to be
 * called 500 ms after the second nowtime has started, because when
 * nowtime is written into the registers of the CMOS clock, it will
 * jump to the next second precisely 500 ms later. Check the Motorola
 * MC146818A or Dallas DS12887 data sheet for details.
 */
static int set_rtc_mmss(unsigned long nowtime)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char save_control, save_freq_select;

	save_control = CMOS_READ(RTC_CONTROL);	/* tell the clock it's being set */
	CMOS_WRITE((save_control | RTC_SET), RTC_CONTROL);

	save_freq_select = CMOS_READ(RTC_FREQ_SELECT);	/* stop and reset prescaler */
	CMOS_WRITE((save_freq_select | RTC_DIV_RESET2), RTC_FREQ_SELECT);

	cmos_minutes = CMOS_READ(RTC_MINUTES);
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		BCD_TO_BIN(cmos_minutes);

	/*
	 * since we're only adjusting minutes and seconds,
	 * don't interfere with hour overflow. This avoids
	 * messing with unknown time zones but requires your
	 * RTC not to be off by more than 15 minutes
	 */
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15) / 30) & 1)
		real_minutes += 30;	/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			BIN_TO_BCD(real_seconds);
			BIN_TO_BCD(real_minutes);
		}
		CMOS_WRITE(real_seconds, RTC_SECONDS);
		CMOS_WRITE(real_minutes, RTC_MINUTES);
	} else {
		printk(KERN_WARNING
		       "set_rtc_mmss: can't update from %d to %d\n",
		       cmos_minutes, real_minutes);
		retval = -1;
	}

	/* The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ...                           -- Markus Kuhn
	 */
	CMOS_WRITE(save_control, RTC_CONTROL);
	CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);

	return retval;
}

/* last time the cmos clock got updated */
static long last_rtc_update;

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
	   
#ifdef CONFIG_PSX_PIO_EXTENSION
	/*
	 * If we have an externally synchronized Linux clock, then update
	 * CMOS clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	read_lock(&xtime_lock);
	if ((time_status & STA_UNSYNC) == 0 && 
		 xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec >= 500000 - (tick >> 1) &&
	    xtime.tv_usec <= 500000 + (tick >> 1))
		if (set_rtc_mmss(xtime.tv_sec) == 0)
			last_rtc_update = xtime.tv_sec;
		else
			last_rtc_update = xtime.tv_sec - 600;	/* do it again in 60 s */
	/* As we return to user mode fire off the other CPU schedulers.. this is
	   basically because we don't yet share IRQ's around. This message is
	   rigged to be safe on the 386 - basically it's a hack, so don't look
	   closely for now.. */
	/*smp_message_pass(MSG_ALL_BUT_SELF, MSG_RESCHEDULE, 0L, 0); */
	read_unlock(&xtime_lock);
#endif	   
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
	unsigned int epoch = 0, year, mon, day, hour, min, sec;
	int i;

#ifdef CONFIG_PSX_PIO_EXTENSION
	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way.
	 */
	/* read RTC exactly on falling edge of update flag */
	for (i = 0 ; i < 1000000 ; i++)	/* may take up to 1 second... */
		if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
			break;
	for (i = 0 ; i < 1000000 ; i++)	/* must try at least 2.228 ms */
		if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
			break;
	do { /* Isn't this overkill ? UIP above should guarantee consistency */
		sec = CMOS_READ(RTC_SECONDS);
		min = CMOS_READ(RTC_MINUTES);
		hour = CMOS_READ(RTC_HOURS);
		day = CMOS_READ(RTC_DAY_OF_MONTH);
		mon = CMOS_READ(RTC_MONTH);
		year = CMOS_READ(RTC_YEAR);
	} while (sec != CMOS_READ(RTC_SECONDS));
	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
	  {
	    BCD_TO_BIN(sec);
	    BCD_TO_BIN(min);
	    BCD_TO_BIN(hour);
	    BCD_TO_BIN(day);
	    BCD_TO_BIN(mon);
	    BCD_TO_BIN(year);
	  }

	/* Attempt to guess the epoch.  This is the same heuristic as in rtc.c so
	   no stupid things will happen to timekeeping.  Who knows, maybe Ultrix
  	   also uses 1952 as epoch ...  */
	if (year >= 20 && year < 48) {
		epoch = 1980;
	} else if (year >= 48 && year < 70) {
		epoch = 1952;
	} else if (year >= 70 && year < 100) {
		epoch = 1928;
	}
	year += epoch;
#else   
	sec = 0;
	min = 0;
	hour = 0;
	day = 31;
	mon = 10;
	year = 2001;
#endif

	write_lock_irq(&xtime_lock);
	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_usec = 0;
	write_unlock_irq(&xtime_lock);

#ifdef CONFIG_PSX_PIO_EXTENSION
   CMOS_WRITE(RTC_REF_CLCK_32KHZ, RTC_REG_A);
#endif
   timer_enable ();
   setup_ps_irq (TIMER2, &irq0);
}

void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;

	read_lock_irqsave(&xtime_lock, flags);
   *tv = xtime;

	/*
	 * xtime is atomically updated in timer_bh. lost_ticks is
	 * nonzero if the timer bottom half hasnt executed yet.
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

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;

	write_unlock_irq(&xtime_lock);
}
