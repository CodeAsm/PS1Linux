/*
 * mcfserial.c -- serial driver for ColdFire internal UARTS.
 *
 * Copyright (c) 1999 Greg Ungerer <gerg@lineo.com>
 * Copyright (c) 2000-2001 Lineo, Inc. <www.lineo.com> 
 *
 * Based on code from 68332serial.c which was:
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 TSHG
 * Copyright (c) 1999 Rt-Control Inc. <jeff@uclinux.org>
 */
 
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/serialP.h>
#ifdef CONFIG_LEDMAN
#include <linux/ledman.h>
#endif
#include <linux/console.h>
#include <linux/version.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/semaphore.h>
#include <asm/bitops.h>
#include <asm/delay.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfuart.h>
#include <asm/nettel.h>
#if LINUX_VERSION_CODE < 0x020100
#define queue_task_irq_off queue_task
#define copy_from_user(a,b,c) memcpy_fromfs(a,b,c)
#define copy_to_user(a,b,c) memcpy_tofs(a,b,c)
#else
#include <asm/uaccess.h>
#endif
#include "mcfserial.h"

/*
 *	the only event we use
 */
#undef RS_EVENT_WRITE_WAKEUP
#define RS_EVENT_WRITE_WAKEUP 0

#if LINUX_VERSION_CODE >= 0x020100
struct timer_list mcfrs_timer_struct;
#endif

/*
 *	Default console port and baud rate...
 */
#ifndef CONSOLE_PORT
#define	CONSOLE_PORT		0
#endif
#ifndef CONSOLE_BAUD_RATE
#define	CONSOLE_BAUD_RATE	9600
#endif

#undef	CONSOLE_BAUD_RATE
#define	CONSOLE_BAUD_RATE	115200 /* DAVIDM remove this */

int mcfrs_console_inited = 0;
int mcfrs_console_port = CONSOLE_PORT;
int mcfrs_console_baud = CONSOLE_BAUD_RATE;


DECLARE_TASK_QUEUE(mcf_tq_serial);

/*
 *	Driver data structures.
 */
struct tty_driver	mcfrs_serial_driver, mcfrs_callout_driver;
static int		mcfrs_serial_refcount;

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
  
/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

/* Debugging...
 */
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW

#define _INLINE_ inline

#define	IRQBASE	224

/*
 *	Configuration table, UARTs to look for at startup.
 */
static struct mcf_serial mcfrs_table[] = {
  { 0, (MCF_MBAR+MCFUART_BASE1), IRQBASE,   ASYNC_BOOT_AUTOCONF },  /* ttyS0 */
  { 0, (MCF_MBAR+MCFUART_BASE2), IRQBASE+1, ASYNC_BOOT_AUTOCONF },  /* ttyS1 */
};


#define	NR_PORTS	(sizeof(mcfrs_table) / sizeof(struct mcf_serial))

static struct tty_struct	*mcfrs_serial_table[NR_PORTS];
static struct termios		*mcfrs_serial_termios[NR_PORTS];
static struct termios		*mcfrs_serial_termios_locked[NR_PORTS];

/*
 * This is used to figure out the divisor speeds and the timeouts.
 */
static int mcfrs_baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400, 460800, 0
};


#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#ifdef CONFIG_MAGIC_SYSRQ
/*
 *	Magic system request keys. Used for debugging...
 */
extern int	magic_sysrq_key(int ch);
#endif


/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char mcfrs_tmp_buf[4096]; /* This is cheating */
#if LINUX_VERSION_CODE < 0x020100
static struct semaphore mcfrs_tmp_buf_sem = MUTEX;
#else
static DECLARE_MUTEX(mcfrs_tmp_buf_sem);
#endif

/*
 *	Forware declarations...
 */
static void	mcfrs_change_speed(struct mcf_serial *info);


static inline int serial_paranoia_check(struct mcf_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo =
		"Warning: null mcf_serial for (%d, %d) in %s\n";

	if (!info) {
		printk(badinfo, MAJOR(device), MINOR(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, MAJOR(device), MINOR(device), routine);
		return 1;
	}
#endif
	return 0;
}

/*
 *	Sets or clears DTR and RTS on the requested line.
 */
static void mcfrs_setsignals(struct mcf_serial *info, int dtr, int rts)
{
	volatile unsigned char	*uartp;
	unsigned long		flags;
	
#if 0
	printk("%s(%d): mcfrs_setsignals(info=%x,dtr=%d,rts=%d)\n",
		__FILE__, __LINE__, info, dtr, rts);
#endif

	save_flags(flags); cli();
	if (dtr >= 0) {
#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
		if (dtr) {
			info->sigs |= TIOCM_DTR;
			ppdata &= ~(info->line ? NETtel_DTR1 : NETtel_DTR0);
		} else {
			info->sigs &= ~TIOCM_DTR;
			ppdata |= (info->line ? NETtel_DTR1 : NETtel_DTR0);
		}
		*((volatile unsigned short *) (MCF_MBAR+MCFSIM_PADAT)) = ppdata;
#endif
	}
	if (rts >= 0) {
		uartp = (volatile unsigned char *) info->addr;
		if (rts) {
			info->sigs |= TIOCM_RTS;
			uartp[MCFUART_UOP1] = MCFUART_UOP_RTS;
		} else {
			info->sigs &= ~TIOCM_RTS;
			uartp[MCFUART_UOP0] = MCFUART_UOP_RTS;
		}
	}
	restore_flags(flags);
	return;
}

/*
 *	Gets values of serial signals.
 */
static int mcfrs_getsignals(struct mcf_serial *info)
{
	volatile unsigned char	*uartp;
	unsigned long		flags;
	int			sigs;
#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
	unsigned short		ppdata;
#endif

#if 0
	printk("%s(%d): mcfrs_getsignals(info=%x)\n", __FILE__, __LINE__);
#endif

	save_flags(flags); cli();
	uartp = (volatile unsigned char *) info->addr;
	sigs = (uartp[MCFUART_UIPR] & MCFUART_UIPR_CTS) ? 0 : TIOCM_CTS;
	sigs |= (info->sigs & TIOCM_RTS);

#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
	ppdata = *((volatile unsigned short *) (MCF_MBAR+MCFSIM_PADAT));
	if (info->line == 0) {
		sigs |= (ppdata & NETtel_DCD0) ? 0 : TIOCM_CD;
		sigs |= (ppdata & NETtel_DTR0) ? 0 : TIOCM_DTR;
	} else if (info->line == 1) {
		sigs |= (ppdata & NETtel_DCD1) ? 0 : TIOCM_CD;
		sigs |= (ppdata & NETtel_DTR1) ? 0 : TIOCM_DTR;
	}
#endif

	restore_flags(flags);
	return(sigs);
}

/*
 * ------------------------------------------------------------
 * mcfrs_stop() and mcfrs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void mcfrs_stop(struct tty_struct *tty)
{
	volatile unsigned char	*uartp;
	struct mcf_serial	*info = (struct mcf_serial *)tty->driver_data;
	unsigned long		flags;

	if (serial_paranoia_check(info, tty->device, "mcfrs_stop"))
		return;
	
	save_flags(flags); cli();
	uartp = (volatile unsigned char *) info->addr;
	info->imr &= ~MCFUART_UIR_TXREADY;
	uartp[MCFUART_UIMR] = info->imr;
	restore_flags(flags);
}

static void mcfrs_start(struct tty_struct *tty)
{
	volatile unsigned char	*uartp;
	struct mcf_serial	*info = (struct mcf_serial *)tty->driver_data;
	unsigned long		flags;
	
	if (serial_paranoia_check(info, tty->device, "mcfrs_start"))
		return;

	save_flags(flags); cli();
	if (info->xmit_cnt && info->xmit_buf) {
		uartp = (volatile unsigned char *) info->addr;
		info->imr |= MCFUART_UIR_TXREADY;
		uartp[MCFUART_UIMR] = info->imr;
	}
	restore_flags(flags);
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * mcfrs_interrupt().  They were separated out for readability's sake.
 *
 * Note: mcfrs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * mcfrs_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 * 
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static _INLINE_ void mcfrs_sched_event(struct mcf_serial *info, int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &mcf_tq_serial);
	mark_bh(CM206_BH);
}

static _INLINE_ void receive_chars(struct mcf_serial *info, struct pt_regs *regs, unsigned short rx)
{
	volatile unsigned char	*uartp;
	struct tty_struct	*tty = info->tty;
	unsigned char		status, ch;

	if (!tty)
		return;

#if defined(CONFIG_LEDMAN)
	ledman_cmd(LEDMAN_CMD_SET, info->line ? LEDMAN_COM2_RX : LEDMAN_COM1_RX);
#endif

	uartp = (volatile unsigned char *) info->addr;

	while ((status = uartp[MCFUART_USR]) & MCFUART_USR_RXREADY) {

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			break;

		ch = uartp[MCFUART_URB];
		info->stats.rx++;

#ifdef CONFIG_MAGIC_SYSRQ
		if (mcfrs_console_inited && (info->line == mcfrs_console_port)) {
			if (magic_sysrq_key(ch))
				continue;
		}
#endif

		tty->flip.count++;
		if (status & MCFUART_USR_RXERR)
			uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETERR;
		if (status & MCFUART_USR_RXBREAK) {
			info->stats.rxbreak++;
			*tty->flip.flag_buf_ptr++ = TTY_BREAK;
		} else if (status & MCFUART_USR_RXPARITY) {
			info->stats.rxparity++;
			*tty->flip.flag_buf_ptr++ = TTY_PARITY;
		} else if (status & MCFUART_USR_RXOVERRUN) {
			info->stats.rxoverrun++;
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
		} else if (status & MCFUART_USR_RXFRAMING) {
			info->stats.rxframing++;
			*tty->flip.flag_buf_ptr++ = TTY_FRAME;
		} else {
			*tty->flip.flag_buf_ptr++ = 0;
		}
		*tty->flip.char_buf_ptr++ = ch;
	}

	queue_task(&tty->flip.tqueue, &tq_timer);
	return;
}

static _INLINE_ void transmit_chars(struct mcf_serial *info)
{
	volatile unsigned char	*uartp;

#if defined(CONFIG_LEDMAN)
	ledman_cmd(LEDMAN_CMD_SET, info->line ? LEDMAN_COM2_TX : LEDMAN_COM1_TX);
#endif

	uartp = (volatile unsigned char *) info->addr;

	if (info->x_char) {
		/* Send special char - probably flow control */
		uartp[MCFUART_UTB] = info->x_char;
		info->x_char = 0;
		info->stats.tx++;
	}

	if ((info->xmit_cnt <= 0) || info->tty->stopped) {
		info->imr &= ~MCFUART_UIR_TXREADY;
		uartp[MCFUART_UIMR] = info->imr;
		return;
	}

	while (uartp[MCFUART_USR] & MCFUART_USR_TXREADY) {
		uartp[MCFUART_UTB] = info->xmit_buf[info->xmit_tail++];
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->stats.tx++;
		if (--info->xmit_cnt <= 0)
			break;
	}

	if (info->xmit_cnt < WAKEUP_CHARS)
		mcfrs_sched_event(info, RS_EVENT_WRITE_WAKEUP);
	return;
}

/*
 * This is the serial driver's generic interrupt routine
 */
void mcfrs_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct mcf_serial	*info;
	unsigned char		isr;

	info = &mcfrs_table[(irq - IRQBASE)];
	isr = (((volatile unsigned char *)info->addr)[MCFUART_UISR]) & info->imr;

	if (isr & MCFUART_UIR_RXREADY)
		receive_chars(info, regs, isr);
	if (isr & MCFUART_UIR_TXREADY)
		transmit_chars(info);
#if 0
	if (isr & MCFUART_UIR_DELTABREAK) {
		printk("%s(%d): delta break!\n", __FILE__, __LINE__);
		receive_chars(info, regs, isr);
	}
#endif

	return;
}

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * mcfrs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using mcfrs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&mcf_tq_serial);
}

static void do_softint(void *private_)
{
	struct mcf_serial	*info = (struct mcf_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
}


/*
 *	Change of state on a DCD line.
 */
void mcfrs_modem_change(struct mcf_serial *info, int dcd)
{
	if (info->count == 0)
		return;

	if (info->flags & ASYNC_CHECK_CD) {
		if (dcd) {
			wake_up_interruptible(&info->open_wait);
		} else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_CALLOUT_NOHUP))) {
			schedule_task(&info->tqueue_hangup);
		}
	}
}


#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)

unsigned short	mcfrs_ppstatus;

/*
 * This subroutine is called when the RS_TIMER goes off. It is used
 * to monitor the state of the DCD lines - since they have no edge
 * sensors and interrupt generators.
 */
static void mcfrs_timer(unsigned long arg)
{
	unsigned short	ppstatus, dcdval;
	int		i;

	ppstatus = *((volatile unsigned short *) (MCF_MBAR + MCFSIM_PADAT)) &
		(NETtel_DCD0 | NETtel_DCD1);

	if (ppstatus != mcfrs_ppstatus) {
		for (i = 0; (i < 2); i++) {
			dcdval = (i ? NETtel_DCD1 : NETtel_DCD0);
			if ((ppstatus & dcdval) != (mcfrs_ppstatus & dcdval)) {
				mcfrs_modem_change(&mcfrs_table[i],
					((ppstatus & dcdval) ? 0 : 1));
			}
		}
	}
	mcfrs_ppstatus = ppstatus;

#if LINUX_VERSION_CODE < 0x020100
	/* Re-arm timer */
	timer_table[RS_TIMER].expires = jiffies + HZ/25;
	timer_active |= 1 << RS_TIMER;
#else
	mcfrs_timer_struct.expires = jiffies + HZ/25;
	add_timer(&mcfrs_timer_struct);
#endif
}

#endif	/* CONFIG_NETtel */


/*
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_serial_hangup() -> tty->hangup() -> mcfrs_hangup()
 * 
 */
static void do_serial_hangup(void *private_)
{
	struct mcf_serial	*info = (struct mcf_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	tty_hangup(tty);
}

static int startup(struct mcf_serial * info)
{
	volatile unsigned char	*uartp;
	unsigned long		flags;
	
	if (info->flags & ASYNC_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
		info->xmit_buf = (unsigned char *) get_free_page(GFP_KERNEL);
		if (!info->xmit_buf)
			return -ENOMEM;
	}

	save_flags(flags); cli();

#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
	/*
	 * Set up poll timer. It is used to check DCD status.
	 */
#if LINUX_VERSION_CODE < 0x020100
	if ((timer_active & (1 << RS_TIMER)) == 0) {
		timer_table[RS_TIMER].expires = jiffies + HZ/25;
		timer_active |= 1 << RS_TIMER;
	}
#endif
#endif

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttyS%d (irq %d)...\n", info->line, info->irq);
#endif

	/*
	 *	Reset UART, get it into known state...
	 */
	uartp = (volatile unsigned char *) info->addr;
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETRX;  /* reset RX */
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETTX;  /* reset TX */
	mcfrs_setsignals(info, 1, 1);

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * and set the speed of the serial port
	 */
	mcfrs_change_speed(info);

	/*
	 * Lastly enable the UART transmitter and receiver, and
	 * interrupt enables.
	 */
	info->imr = MCFUART_UIR_RXREADY;
	uartp[MCFUART_UCR] = MCFUART_UCR_RXENABLE | MCFUART_UCR_TXENABLE;
	uartp[MCFUART_UIMR] = info->imr;

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct mcf_serial * info)
{
	volatile unsigned char	*uartp;
	unsigned long		flags;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....\n", info->line,
	       info->irq);
#endif
	
	save_flags(flags); cli(); /* Disable interrupts */

	uartp = (volatile unsigned char *) info->addr;
	uartp[MCFUART_UIMR] = 0;  /* mask all interrupts */
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETRX;  /* reset RX */
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETTX;  /* reset TX */

	if (!info->tty || (info->tty->termios->c_cflag & HUPCL))
		mcfrs_setsignals(info, 0, 0);

	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);
	
	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}


/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void mcfrs_change_speed(struct mcf_serial *info)
{
	volatile unsigned char	*uartp;
	unsigned int		baudclk, cflag;
	unsigned long		flags;
	unsigned char		mr1, mr2;
	int			i;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (info->addr == 0)
		return;

#if 0
	printk("%s(%d): mcfrs_change_speed()\n", __FILE__, __LINE__);
#endif

	i = cflag & CBAUD;
i = B115200;
	if (i & CBAUDEX) {
		i &= ~CBAUDEX;
		if (i < 1 || i > 4)
			info->tty->termios->c_cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	if (i == 0) {
		mcfrs_setsignals(info, 0, -1);
		return;
	}
	baudclk = ((MCF_CLK / 32) / mcfrs_baud_table[i]);
	info->baud = mcfrs_baud_table[i];

	mr1 = MCFUART_MR1_RXIRQRDY | MCFUART_MR1_RXERRCHAR;
	mr2 = 0;

	switch (cflag & CSIZE) {
	case CS5:	mr1 |= MCFUART_MR1_CS5; break;
	case CS6:	mr1 |= MCFUART_MR1_CS6; break;
	case CS7:	mr1 |= MCFUART_MR1_CS7; break;
	case CS8:
	default:	mr1 |= MCFUART_MR1_CS8; break;
	}

	if (cflag & PARENB) {
		if (cflag & PARODD)
			mr1 |= MCFUART_MR1_PARITYODD;
		else
			mr1 |= MCFUART_MR1_PARITYEVEN;
	} else {
		mr1 |= MCFUART_MR1_PARITYNONE;
	}

	if (cflag & CSTOPB)
		mr2 |= MCFUART_MR2_STOP2;
	else
		mr2 |= MCFUART_MR2_STOP1;

	if (cflag & CRTSCTS) {
		mr1 |= MCFUART_MR1_RXRTS;
		mr2 |= MCFUART_MR2_TXCTS;
	}

	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else
		info->flags |= ASYNC_CHECK_CD;

	uartp = (volatile unsigned char *) info->addr;

	save_flags(flags); cli();
#if 0
	printk("%s(%d): mr1=%x mr2=%x baudclk=%x\n", __FILE__, __LINE__,
		mr1, mr2, baudclk);
#endif
	/*
	  Note: pg 12-16 of MCF5206e User's Manual states that a
	  software reset should be performed prior to changing
	  UMR1,2, UCSR, UACR, bit 7
	*/
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETRX;    /* reset RX */
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETTX;    /* reset TX */
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETMRPTR;	/* reset MR pointer */
	uartp[MCFUART_UMR] = mr1;
	uartp[MCFUART_UMR] = mr2;
	uartp[MCFUART_UBG1] = (baudclk & 0xff00) >> 8;	/* set msb byte */
	uartp[MCFUART_UBG2] = (baudclk & 0xff);		/* set lsb byte */
	uartp[MCFUART_UCSR] = MCFUART_UCSR_RXCLKTIMER | MCFUART_UCSR_TXCLKTIMER;
	uartp[MCFUART_UCR] = MCFUART_UCR_RXENABLE | MCFUART_UCR_TXENABLE;
	mcfrs_setsignals(info, 1, -1);
	restore_flags(flags);
	return;
}

static void mcfrs_flush_chars(struct tty_struct *tty)
{
	volatile unsigned char	*uartp;
	struct mcf_serial	*info = (struct mcf_serial *)tty->driver_data;
	unsigned long		flags;

	if (serial_paranoia_check(info, tty->device, "mcfrs_flush_chars"))
		return;

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	/* Enable transmitter */
	save_flags(flags); cli();
	uartp = (volatile unsigned char *) info->addr;
	info->imr |= MCFUART_UIR_TXREADY;
	uartp[MCFUART_UIMR] = info->imr;
	restore_flags(flags);
}

static int mcfrs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	volatile unsigned char	*uartp;
	struct mcf_serial	*info = (struct mcf_serial *)tty->driver_data;
	unsigned long		flags;
	int			c, total = 0;

#if 0
	printk("%s(%d): mcfrs_write(tty=%x,from_user=%d,buf=%x,count=%d)\n",
		__FILE__, __LINE__, tty, from_user, buf, count);
#endif

	if (serial_paranoia_check(info, tty->device, "mcfrs_write"))
		return 0;

	if (!tty || !info->xmit_buf)
		return 0;
	
	save_flags(flags);
	while (1) {
		cli();		
		c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				   SERIAL_XMIT_SIZE - info->xmit_head));

		if (c <= 0) {
			restore_flags(flags);
			break;
		}

		if (from_user) {
			down(&mcfrs_tmp_buf_sem);
			copy_from_user(mcfrs_tmp_buf, buf, c);
			restore_flags(flags);
			cli();		
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, mcfrs_tmp_buf, c);
			up(&mcfrs_tmp_buf_sem);
		} else
			memcpy(info->xmit_buf + info->xmit_head, buf, c);

		info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt += c;
		restore_flags(flags);

		buf += c;
		count -= c;
		total += c;
	}

	cli();
	uartp = (volatile unsigned char *) info->addr;
	info->imr |= MCFUART_UIR_TXREADY;
	uartp[MCFUART_UIMR] = info->imr;
	restore_flags(flags);

	return total;
}

static int mcfrs_write_room(struct tty_struct *tty)
{
	struct mcf_serial *info = (struct mcf_serial *)tty->driver_data;
	int	ret;
				
	if (serial_paranoia_check(info, tty->device, "mcfrs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}

static int mcfrs_chars_in_buffer(struct tty_struct *tty)
{
	struct mcf_serial *info = (struct mcf_serial *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "mcfrs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void mcfrs_flush_buffer(struct tty_struct *tty)
{
	struct mcf_serial	*info = (struct mcf_serial *)tty->driver_data;
	unsigned long		flags;
				
	if (serial_paranoia_check(info, tty->device, "mcfrs_flush_buffer"))
		return;

	save_flags(flags); cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	restore_flags(flags);

	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/*
 * ------------------------------------------------------------
 * mcfrs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void mcfrs_throttle(struct tty_struct * tty)
{
	struct mcf_serial *info = (struct mcf_serial *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "mcfrs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off RTS line (do this atomic) */
}

static void mcfrs_unthrottle(struct tty_struct * tty)
{
	struct mcf_serial *info = (struct mcf_serial *)tty->driver_data;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", _tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "mcfrs_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			info->x_char = START_CHAR(tty);
	}

	/* Assert RTS line (do this atomic) */
}

/*
 * ------------------------------------------------------------
 * mcfrs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct mcf_serial * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;
  
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = info->addr;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	copy_to_user(retinfo,&tmp,sizeof(*retinfo));
	return 0;
}

static int set_serial_info(struct mcf_serial * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct mcf_serial old_info;
	int 			retval = 0;

	if (!new_info)
		return -EFAULT;
	copy_from_user(&new_serial,new_info,sizeof(new_serial));
	old_info = *info;

	if (!suser()) {
		if ((new_serial.baud_base != info->baud_base) ||
		    (new_serial.type != info->type) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (info->flags & ~ASYNC_USR_MASK)))
			return -EPERM;
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
		info->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}

	if (info->count > 1)
		return -EBUSY;

	/*
	 * OK, past this point, all the error checking has been done.
	 * At this point, we start making changes.....
	 */

	info->baud_base = new_serial.baud_base;
	info->flags = ((info->flags & ~ASYNC_FLAGS) |
			(new_serial.flags & ASYNC_FLAGS));
	info->type = new_serial.type;
	info->close_delay = new_serial.close_delay;
	info->closing_wait = new_serial.closing_wait;

check_and_exit:
	retval = startup(info);
	return retval;
}

/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 */
static int get_lsr_info(struct mcf_serial * info, unsigned int *value)
{
	volatile unsigned char	*uartp;
	unsigned long		flags;
	unsigned char		status;

	save_flags(flags); cli();
	uartp = (volatile unsigned char *) info->addr;
	status = (uartp[MCFUART_USR] & MCFUART_USR_TXEMPTY) ? TIOCSER_TEMT : 0;
	restore_flags(flags);

	put_user(status,value);
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(	struct mcf_serial * info, int duration)
{
	volatile unsigned char	*uartp;
	unsigned long		flags;

	if (!info->addr)
		return;
	current->state = TASK_INTERRUPTIBLE;
#if LINUX_VERSION_CODE < 0x020100
	current->timeout = jiffies + duration;
#endif
	uartp = (volatile unsigned char *) info->addr;

	save_flags(flags); cli();
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDBREAKSTART;
#if LINUX_VERSION_CODE < 0x020100
	schedule();
#else
	schedule_timeout(jiffies + duration);
#endif
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDBREAKSTOP;
	restore_flags(flags);
}

static int mcfrs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct mcf_serial * info = (struct mcf_serial *)tty->driver_data;
	unsigned int val;
	int retval, error;
	int dtr, rts;

	if (serial_paranoia_check(info, tty->device, "mcfrs_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGWILD)  &&
	    (cmd != TIOCSERSWILD) && (cmd != TIOCSERGSTRUCT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}
	
	switch (cmd) {
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (!arg)
				send_break(info, HZ/4);	/* 1/4 second */
			return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			send_break(info, arg ? arg*(HZ/10) : HZ/4);
			return 0;
		case TIOCGSOFTCAR:
			error = verify_area(VERIFY_WRITE, (void *) arg,sizeof(long));
			if (error)
				return error;
			put_user(C_CLOCAL(tty) ? 1 : 0,
				    (unsigned long *) arg);
			return 0;
		case TIOCSSOFTCAR:
			get_user(arg, (unsigned long *) arg);
			tty->termios->c_cflag =
				((tty->termios->c_cflag & ~CLOCAL) |
				 (arg ? CLOCAL : 0));
			return 0;
		case TIOCGSERIAL:
			error = verify_area(VERIFY_WRITE, (void *) arg,
						sizeof(struct serial_struct));
			if (error)
				return error;
			return get_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSSERIAL:
			return set_serial_info(info,
					       (struct serial_struct *) arg);
		case TIOCSERGETLSR: /* Get line status register */
			error = verify_area(VERIFY_WRITE, (void *) arg,
				sizeof(unsigned int));
			if (error)
				return error;
			else
			    return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			error = verify_area(VERIFY_WRITE, (void *) arg,
						sizeof(struct mcf_serial));
			if (error)
				return error;
			copy_to_user((struct mcf_serial *) arg,
				    info, sizeof(struct mcf_serial));
			return 0;
			
		case TIOCMGET:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
                                return(error);
			val = mcfrs_getsignals(info);
			put_user(val, (unsigned int *) arg);
			break;

                case TIOCMBIS:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
				return(error);

#if LINUX_VERSION_CODE < 0x020100
			val = get_user((unsigned int *) arg);
#else
			get_user(val, (unsigned int *) arg);
#endif
			rts = (val & TIOCM_RTS) ? 1 : -1;
			dtr = (val & TIOCM_DTR) ? 1 : -1;
			mcfrs_setsignals(info, dtr, rts);
			break;

                case TIOCMBIC:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
				return(error);
#if LINUX_VERSION_CODE < 0x020100
			val = get_user((unsigned int *) arg);
#else
			get_user(val, (unsigned int *) arg);
#endif
			rts = (val & TIOCM_RTS) ? 0 : -1;
			dtr = (val & TIOCM_DTR) ? 0 : -1;
			mcfrs_setsignals(info, dtr, rts);
			break;

                case TIOCMSET:
			if ((error = verify_area(VERIFY_WRITE, (void *) arg,
                            sizeof(unsigned int))))
				return(error);
#if LINUX_VERSION_CODE < 0x020100
			val = get_user((unsigned int *) arg);
#else
			get_user(val, (unsigned int *) arg);
#endif
			rts = (val & TIOCM_RTS) ? 1 : 0;
			dtr = (val & TIOCM_DTR) ? 1 : 0;
			mcfrs_setsignals(info, dtr, rts);
			break;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void mcfrs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct mcf_serial *info = (struct mcf_serial *)tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	mcfrs_change_speed(info);

	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		mcfrs_setsignals(info, -1, 1);
#if 0
		mcfrs_start(tty);
#endif
	}
}

/*
 * ------------------------------------------------------------
 * mcfrs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * S structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void mcfrs_close(struct tty_struct *tty, struct file * filp)
{
	volatile unsigned char	*uartp;
	struct mcf_serial	*info = (struct mcf_serial *)tty->driver_data;
	unsigned long		flags;

	if (!info || serial_paranoia_check(info, tty->device, "mcfrs_close"))
		return;
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
	
#ifdef SERIAL_DEBUG_OPEN
	printk("mcfrs_close ttyS%d, count = %d\n", info->line, info->count);
#endif
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("mcfrs_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("mcfrs_close: bad serial port count for ttyS%d: %d\n",
		       info->line, info->count);
		info->count = 0;
	}
	if (info->count) {
		restore_flags(flags);
		return;
	}
	info->flags |= ASYNC_CLOSING;

	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;

	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);

	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	info->imr &= ~MCFUART_UIR_RXREADY;
	uartp = (volatile unsigned char *) info->addr;
	uartp[MCFUART_UIMR] = info->imr;

#if 0
	/* FIXME: do we need to keep this enabled for console?? */
	if (mcfrs_console_inited && (mcfrs_console_port == info->line)) {
		/* Do not disable the UART */ ;
	} else
#endif
	shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (tty->ldisc.num != ldiscs[N_TTY].num) {
		if (tty->ldisc.close)
			(tty->ldisc.close)(tty);
		tty->ldisc = ldiscs[N_TTY];
		tty->termios->c_line = N_TTY;
		if (tty->ldisc.open)
			(tty->ldisc.open)(tty);
	}
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
#if LINUX_VERSION_CODE < 0x020100
			current->timeout = jiffies + info->close_delay;
			schedule();
#else
			schedule_timeout(info->close_delay);
#endif
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	restore_flags(flags);
}

/*
 * mcfrs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void mcfrs_hangup(struct tty_struct *tty)
{
	struct mcf_serial * info = (struct mcf_serial *)tty->driver_data;
	
	if (serial_paranoia_check(info, tty->device, "mcfrs_hangup"))
		return;
	
	mcfrs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * mcfrs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct mcf_serial *info)
{
#if LINUX_VERSION_CODE < 0x020100
	struct wait_queue wait = { current, NULL };
#else
	DECLARE_WAITQUEUE(wait, current);
#endif
	int		retval;
	int		do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (info->flags & ASYNC_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & ASYNC_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
#else
		return -EAGAIN;
#endif
	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
		if (info->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}
	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, info->count is dropped by one, so that
	 * mcfrs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttyS%d, count = %d\n",
	       info->line, info->count);
#endif
	info->count--;
	info->blocked_open++;
	while (1) {
		cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE))
			mcfrs_setsignals(info, 1, 1);
		sti();
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) && do_clocal)
			break;
#if LINUX_VERSION_CODE < 0x020100
		if (current->signal & ~current->blocked) {
#else
		if (signal_pending(current)) {
#endif
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttyS%d, count = %d\n",
		       info->line, info->count);
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttyS%d, count = %d\n",
	       info->line, info->count);
#endif
	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}	

/*
 * This routine is called whenever a serial port is opened. It
 * enables interrupts for a serial port, linking in its structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
int mcfrs_open(struct tty_struct *tty, struct file * filp)
{
	struct mcf_serial	*info;
	int 			retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;
	info = mcfrs_table + line;
	if (serial_paranoia_check(info, tty->device, "mcfrs_open"))
		return -ENODEV;
#ifdef SERIAL_DEBUG_OPEN
	printk("mcfrs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->count);
#endif
	info->count++;
	tty->driver_data = info;
	info->tty = tty;

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("mcfrs_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

	if ((info->count == 1) && (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else 
			*tty->termios = info->callout_termios;
		mcfrs_change_speed(info);
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("mcfrs_open ttyS%d successful...\n", info->line);
#endif
	return 0;
}

/*
 *	Based on the line number set up the internal interrupt stuff.
 */
static void mcfrs_irqinit(struct mcf_serial *info)
{
	volatile unsigned char	*icrp, *uartp;

	switch (info->line) {
	case 0:
		icrp = (volatile unsigned char *) (MCF_MBAR + MCFSIM_UART1ICR);
		*icrp = /*MCFSIM_ICR_AUTOVEC |*/ MCFSIM_ICR_LEVEL6 |
			MCFSIM_ICR_PRI1;
		mcf_setimr(mcf_getimr() & ~MCFSIM_IMR_UART1);
		break;
	case 1:
		icrp = (volatile unsigned char *) (MCF_MBAR + MCFSIM_UART2ICR);
		*icrp = /*MCFSIM_ICR_AUTOVEC |*/ MCFSIM_ICR_LEVEL6 |
			MCFSIM_ICR_PRI2;
		mcf_setimr(mcf_getimr() & ~MCFSIM_IMR_UART2);
		break;
	default:
		printk("SERIAL: don't know how to handle UART %d interrupt?\n",
			info->line);
		return;
	}

	uartp = (volatile unsigned char *) info->addr;
	uartp[MCFUART_UIVR] = info->irq;

	if (request_irq(info->irq, mcfrs_interrupt, SA_INTERRUPT,
	    "ColdFire UART", NULL)) {
		printk("SERIAL: Unable to attach ColdFire UART %d interrupt "
			"vector=%d\n", info->line, info->irq);
	}

	return;
}


char *mcfrs_drivername = "ColdFire internal UART serial driver version 1.00\n";


/*
 * Serial stats reporting...
 */
int mcfrs_readproc(char *buffer)
{
	struct mcf_serial	*info;
	char			str[20];
	int			len, sigs, i;

	len = sprintf(buffer, mcfrs_drivername);
	for (i = 0; (i < NR_PORTS); i++) {
		info = &mcfrs_table[i];
		len += sprintf((buffer + len), "%d: port:%x irq=%d baud:%d ",
			i, info->addr, info->irq, info->baud);
		if (info->stats.rx || info->stats.tx)
			len += sprintf((buffer + len), "tx:%d rx:%d ",
			info->stats.tx, info->stats.rx);
		if (info->stats.rxframing)
			len += sprintf((buffer + len), "fe:%d ",
			info->stats.rxframing);
		if (info->stats.rxparity)
			len += sprintf((buffer + len), "pe:%d ",
			info->stats.rxparity);
		if (info->stats.rxbreak)
			len += sprintf((buffer + len), "brk:%d ",
			info->stats.rxbreak);
		if (info->stats.rxoverrun)
			len += sprintf((buffer + len), "oe:%d ",
			info->stats.rxoverrun);

		str[0] = str[1] = 0;
		if ((sigs = mcfrs_getsignals(info))) {
			if (sigs & TIOCM_RTS)
				strcat(str, "|RTS");
			if (sigs & TIOCM_CTS)
				strcat(str, "|CTS");
			if (sigs & TIOCM_DTR)
				strcat(str, "|DTR");
			if (sigs & TIOCM_CD)
				strcat(str, "|CD");
		}

		len += sprintf((buffer + len), "%s\n", &str[1]);
	}

	return(len);
}


/* Finally, routines used to initialize the serial driver. */

static void show_serial_version(void)
{
	printk(mcfrs_drivername);
}

/* mcfrs_init inits the driver */
static int __init
mcfrs_init(void)
{
	struct mcf_serial	*info;
	unsigned long		flags;
	int			i;

	init_bh(CM206_BH, do_serial_bh);

	/* Setup base handler, and timer table. */
#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
#if LINUX_VERSION_CODE < 0x020100
	timer_table[RS_TIMER].fn = mcfrs_timer;
	timer_table[RS_TIMER].expires = 0;
#else
	init_timer(&mcfrs_timer_struct);
	mcfrs_timer_struct.function = mcfrs_timer;
	mcfrs_timer_struct.data = 0;
	mcfrs_timer_struct.expires = jiffies + HZ/25;
	add_timer(&mcfrs_timer_struct);
#endif
#endif

	show_serial_version();

	/* Initialize the tty_driver structure */
	memset(&mcfrs_serial_driver, 0, sizeof(struct tty_driver));
	mcfrs_serial_driver.magic = TTY_DRIVER_MAGIC;
	mcfrs_serial_driver.name = "ttyS";
	mcfrs_serial_driver.major = TTY_MAJOR;
	mcfrs_serial_driver.minor_start = 64;
	mcfrs_serial_driver.num = NR_PORTS;
	mcfrs_serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	mcfrs_serial_driver.subtype = SERIAL_TYPE_NORMAL;
	mcfrs_serial_driver.init_termios = tty_std_termios;

	mcfrs_serial_driver.init_termios.c_cflag =
		B115200 /* DAVIDM 9600 */ | CS8 | CREAD | HUPCL | CLOCAL;
	mcfrs_serial_driver.flags = TTY_DRIVER_REAL_RAW;
	mcfrs_serial_driver.refcount = &mcfrs_serial_refcount;
	mcfrs_serial_driver.table = mcfrs_serial_table;
	mcfrs_serial_driver.termios = mcfrs_serial_termios;
	mcfrs_serial_driver.termios_locked = mcfrs_serial_termios_locked;

	mcfrs_serial_driver.open = mcfrs_open;
	mcfrs_serial_driver.close = mcfrs_close;
	mcfrs_serial_driver.write = mcfrs_write;
	mcfrs_serial_driver.flush_chars = mcfrs_flush_chars;
	mcfrs_serial_driver.write_room = mcfrs_write_room;
	mcfrs_serial_driver.chars_in_buffer = mcfrs_chars_in_buffer;
	mcfrs_serial_driver.flush_buffer = mcfrs_flush_buffer;
	mcfrs_serial_driver.ioctl = mcfrs_ioctl;
	mcfrs_serial_driver.throttle = mcfrs_throttle;
	mcfrs_serial_driver.unthrottle = mcfrs_unthrottle;
	mcfrs_serial_driver.set_termios = mcfrs_set_termios;
	mcfrs_serial_driver.stop = mcfrs_stop;
	mcfrs_serial_driver.start = mcfrs_start;
	mcfrs_serial_driver.hangup = mcfrs_hangup;

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	mcfrs_callout_driver = mcfrs_serial_driver;
	mcfrs_callout_driver.name = "cua";
	mcfrs_callout_driver.major = TTYAUX_MAJOR;
	mcfrs_callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&mcfrs_serial_driver)) {
		printk(__FUNCTION__ ": Couldn't register serial driver\n");
		return(-EBUSY);
	}
	if (tty_register_driver(&mcfrs_callout_driver)) {
		printk(__FUNCTION__ ": Couldn't register callout driver\n");
		return(-EBUSY);
	}
	
	save_flags(flags); cli();

	/*
	 *	Configure all the attached serial ports.
	 */
	for (i = 0, info = mcfrs_table; (i < NR_PORTS); i++, info++) {
		info->magic = SERIAL_MAGIC;
		info->line = i;
		info->tty = 0;
		info->custom_divisor = 16;
		info->close_delay = 50;
		info->closing_wait = 3000;
		info->x_char = 0;
		info->event = 0;
		info->count = 0;
		info->blocked_open = 0;
		info->tqueue.routine = do_softint;
		info->tqueue.data = info;
		info->tqueue_hangup.routine = do_serial_hangup;
		info->tqueue_hangup.data = info;
		info->callout_termios = mcfrs_callout_driver.init_termios;
		info->normal_termios = mcfrs_serial_driver.init_termios;
#if LINUX_VERSION_CODE < 0x020100
		info->open_wait = 0;
		info->close_wait = 0;
#else
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
#endif

		mcfrs_setsignals(info, 0, 0);
		mcfrs_irqinit(info);

		printk("%s%d at 0x%04x (irq = %d)", mcfrs_serial_driver.name,
			info->line, info->addr, info->irq);
		printk(" is a builtin ColdFire UART\n");
	}

	restore_flags(flags);
	return 0;
}

module_init(mcfrs_init);
/* DAVIDM module_exit(mcfrs_fini); */

/****************************************************************************/
/*                          Serial Console                                  */
/****************************************************************************/
/*
 *	Quick and dirty UART initialization, for console output.
 */

void mcfrs_console_init(void)
{
	volatile unsigned char	*uartp;
	unsigned int		clk;

	/*
	 *	Reset UART, get it into known state...
	 */
	uartp = (volatile unsigned char *) (MCF_MBAR +
		(mcfrs_console_port ? MCFUART_BASE2 : MCFUART_BASE1));

	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETRX;  /* reset RX */
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETTX;  /* reset TX */
	uartp[MCFUART_UCR] = MCFUART_UCR_CMDRESETMRPTR;  /* reset MR pointer */

	/*
	 * Set port for defined baud , 8 data bits, 1 stop bit, no parity.
	 */
	uartp[MCFUART_UMR] = MCFUART_MR1_PARITYNONE | MCFUART_MR1_CS8;
	uartp[MCFUART_UMR] = MCFUART_MR2_STOP1;

	clk = ((MCF_CLK / 32) / mcfrs_console_baud);  /* Set baud above */
	uartp[MCFUART_UBG1] = (clk & 0xff00) >> 8;  /* set msb baud */
	uartp[MCFUART_UBG2] = (clk & 0xff);  /* set lsb baud */

	uartp[MCFUART_UCSR] = MCFUART_UCSR_RXCLKTIMER | MCFUART_UCSR_TXCLKTIMER;
	uartp[MCFUART_UCR] = MCFUART_UCR_RXENABLE | MCFUART_UCR_TXENABLE;

	mcfrs_console_inited++;
	return;
}


/*
 *	Setup for console. Argument comes from the boot command line.
 */

int mcfrs_console_setup(struct console *cp, char *arg)
{
	if (!cp)
		return(-1);

	if (!strncmp(cp->name, "ttyS", 4))
		mcfrs_console_port = cp->name[4] - '0';
	else if (!strncmp(cp->name, "cua", 3))
		mcfrs_console_port = cp->name[3] - '0';
	else
		return(-1);
	if (arg)
		mcfrs_console_baud = simple_strtoul(arg,NULL,0);
	mcfrs_console_init(); /* make sure baud rate changes */
	return(0);
}


static kdev_t mcfrs_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}


/*
 *	Output a single character, using UART polled mode.
 *	This is ised for console output.
 */

void mcfrs_put_char(char ch)
{
	volatile unsigned char	*uartp;
	unsigned long		flags;
	int			i;

	uartp = (volatile unsigned char *) (MCF_MBAR +
		(mcfrs_console_port ? MCFUART_BASE2 : MCFUART_BASE1));

	save_flags(flags); cli();
	for (i = 0; (i < 0x10000); i++) {
		if (uartp[MCFUART_USR] & MCFUART_USR_TXREADY)
			break;
	}
	if (i < 0x10000) {
		uartp[MCFUART_UTB] = ch;
		for (i = 0; (i < 0x10000); i++)
			if (uartp[MCFUART_USR] & MCFUART_USR_TXEMPTY)
				break;
	}
	if (i >= 0x10000)
		mcfrs_console_init(); /* try and get it back */
	restore_flags(flags);

	return;
}


/*
 * rs_console_write is registered for printk output.
 */

void mcfrs_console_write(struct console *cp, const char *p, unsigned len)
{
	if (!mcfrs_console_inited)
		mcfrs_console_init();
	while (len-- > 0) {
		if (*p == '\n')
			mcfrs_put_char('\r');
		mcfrs_put_char(*p++);
	}
}

/*
 * declare our consoles
 */

struct console mcfrs_console0 = {
	name:		"ttyS0",
	write:		mcfrs_console_write,
	read:		NULL,
	device:		mcfrs_console_device,
	wait_key:	NULL,
	unblank:	NULL,
	setup:		mcfrs_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
	cflag:		0,
	next:		NULL
};

struct console mcfrs_console1 = {
	name:		"ttyS1",
	write:		mcfrs_console_write,
	read:		NULL,
	device:		NULL,
	wait_key:	NULL,
	unblank:	NULL,
	setup:		mcfrs_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
	cflag:		0,
	next:		NULL
};

/****************************************************************************/
