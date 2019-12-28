/***************************************************************************/

/*
 *	linux/drivers/char/mcfwatchdog.c
 *
 *	Copyright (C) 1999-2000, Greg Ungerer (gerg@lineo.com)
 * 	Copyright (C) 2000  Lineo Inc. (www.lineo.com)  
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcftimer.h>
#include <asm/mcfsim.h>
#include <asm/irq.h>
#include <asm/delay.h>

/***************************************************************************/

/*
 *	Define the watchdog vector.
 */
#define	IRQ_WATCHDOG	250

/***************************************************************************/

void	watchdog_alive(unsigned long arg);
void	watchdog_init(void);
void	watchdog_timeout(int irq, void *dummy, struct pt_regs *fp);

extern void	dump(struct pt_regs *fp);


/*
 *	Data for registering the watchdog alive routine with ticker.
 */
struct timer_list	watchdog_timerlist = {
	NULL, NULL, 0, 0, watchdog_alive
};


#if CONFIG_OLDMASK
/*
 *	The old mask 5307 has a broken watchdog timer. It will interrupt
 *	you regardless of writing to its "alive" register. It can still
 *	be useful but you have to play some tricks with it. This code
 *	supports a clock ticker timeout. If the right number of clock
 *	ticks are not counted then it is assumed that the watchdog saved
 *	us from a bad bus cycle.
 */
#define	SWTREF_COUNT	25

int	swt_inwatchdog = 0;		/* Has watchdog expired */
int	swt_doit = 0;			/* Start delay before tripping */
int	swt_lastjiffies = 0;		/* Tick count at last watchdog */
int	swt_reference = SWTREF_COUNT;	/* Refereence tick count */
#endif

/***************************************************************************/

/*
 *	Software Watchdog Timer enable. Seems to be the same across all
 *	ColdFire CPu members.
 */
void watchdog_enable(void)
{
	volatile unsigned char	*mbar;

	mbar = (volatile unsigned char *) MCF_MBAR;
	*(mbar + MCFSIM_SWSR) = 0x55;
	*(mbar + MCFSIM_SWSR) = 0xaa;
	*(mbar + MCFSIM_SYPCR) = 0xbe;
}

/***************************************************************************/

void watchdog_disable(void)
{
	volatile unsigned char	*mbar;

	mbar = (volatile unsigned char *) MCF_MBAR;
	*(mbar + MCFSIM_SWSR) = 0x55;
	*(mbar + MCFSIM_SWSR) = 0xaa;
	*(mbar + MCFSIM_SYPCR) = 0x00 /*0x3e*/;
}

/***************************************************************************/

#define	TIMEDELAY	45

/*
 *	Process a watchdog timeout interrupt. For a normal clean watchdog
 *	we just do a process dump. For old broken 5307 we need to verify
 *	if this was a real watchdog event or not...
 */
void watchdog_timeout(int irq, void *dummy, struct pt_regs *fp)
{
#ifdef CONFIG_OLDMASK
	/*
	 *	Debuging code for software watchdog. If we get in here
	 *	and timer interrupt counts don't match we know that a
	 *	bad external bus cycle must have locked the CPU.
	 */
	if ((swt_doit++ > TIMEDELAY) &&
	    ((swt_lastjiffies + swt_reference) > jiffies)) {
		if (swt_inwatchdog) {
			cli();
			watchdog_disable();
			mcf_setimr(mcf_getimr() | MCFSIM_IMR_SWD);
			printk("%s(%d): Double WATCHDOG PANIC!!\n",
				__FILE__, __LINE__);
			for (;;)
				;
		}

		swt_inwatchdog++;
		swt_doit = TIMEDELAY - 8;	/* 8 seconds grace */
		printk("WATCHDOG: expired last=%d(%d) jiffies=%d!\n",
			swt_lastjiffies, swt_reference, jiffies);
		dump(fp);
		force_sig(SIGSEGV, current);
		swt_inwatchdog  = 0;
	}
	swt_lastjiffies = jiffies;
#else
	printk("WATCHDOG: expired!\n");
	dump(fp);
#endif /* CONFIG_OLDMASK */
}

/***************************************************************************/

void watchdog_init(void)
{
	volatile unsigned char	*mbar;

	printk("WATCHDOG: initializing at vector=%d\n", IRQ_WATCHDOG);
	request_irq(IRQ_WATCHDOG, watchdog_timeout, SA_INTERRUPT,
		"Watchdog Timer", NULL);

	watchdog_timerlist.expires = jiffies + 1;
	add_timer(&watchdog_timerlist);

	mbar = (volatile unsigned char *) MCF_MBAR;
	*(mbar + MCFSIM_SWDICR) = MCFSIM_ICR_LEVEL1 | MCFSIM_ICR_PRI3;
	*(mbar + MCFSIM_SWIVR) = IRQ_WATCHDOG;
	mcf_setimr(mcf_getimr() & ~MCFSIM_IMR_SWD);
	watchdog_enable();
}

/***************************************************************************/

void watchdog_alive(unsigned long arg)
{
	volatile unsigned char	*mbar;

	mbar = (volatile unsigned char *) MCF_MBAR;
	*(mbar + MCFSIM_SWSR) = 0x55;
	*(mbar + MCFSIM_SWSR) = 0xaa;

	/* Re-arm the watchdog alive poll */
	watchdog_timerlist.expires = jiffies + 1;
	add_timer(&watchdog_timerlist);
}

/***************************************************************************/
