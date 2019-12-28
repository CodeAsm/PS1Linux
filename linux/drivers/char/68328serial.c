/* 68328serial.c: Serial port driver for 68328 microcontroller
 *
 * Copyright (C) 1995       David S. Miller    <davem@caip.rutgers.edu>
 * Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 * Copyright (C) 1998, 1999 D. Jeff Dionne     <jeff@uclinux.org>
 * Copyright (C) 1999       Vladimir Gurevich  <vgurevic@cisco.com>
 *
 */
 
#include <asm/dbg.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/reboot.h>
#include <linux/keyboard.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/delay.h>
#include <asm/uaccess.h>
#include <asm/MC68328.h>

#include "68328serial.h"

/* Turn off usage of real serial interrupt code, to "support" Copilot */
#ifdef CONFIG_XCOPILOT_BUGS
#undef USE_INTS
#else
#define USE_INTS
#endif

static struct m68k_serial m68k_soft;

struct tty_struct m68k_ttys;
struct m68k_serial *m68k_consinfo = 0;

#define M68K_CLOCK (16667000) /* FIXME: 16MHz is likely wrong */

DECLARE_TASK_QUEUE(tq_serial);

#ifdef CONFIG_CONSOLE
extern wait_queue_head_t keypress_wait; 
#endif

struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
  
/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

/* Debugging... DEBUG_INTR is bad to use when one of the zs
 * lines is your console ;(
 */
#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW

#define RS_ISR_PASS_LIMIT 256

#define _INLINE_ inline

static void change_speed(struct m68k_serial *info);

static struct tty_struct *serial_table[2];
static struct termios *serial_termios[2];
static struct termios *serial_termios_locked[2];

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/*
 *	Setup for console. Argument comes from the boot command line.
 */

#if defined(CONFIG_M68EZ328ADS) || defined(CONFIG_ALMA_ANS)
#define	CONSOLE_BAUD_RATE	115200
#define	DEFAULT_CBAUD		B115200
#endif

#ifndef CONSOLE_BAUD_RATE
#define	CONSOLE_BAUD_RATE	9600
#define	DEFAULT_CBAUD		B9600
#endif

static int m68328_console_initted = 0;
static int m68328_console_baud    = CONSOLE_BAUD_RATE;
static int m68328_console_cbaud   = DEFAULT_CBAUD;


/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char tmp_buf[SERIAL_XMIT_SIZE]; /* This is cheating */
DECLARE_MUTEX(tmp_buf_sem);

static inline int serial_paranoia_check(struct m68k_serial *info,
					dev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%d, %d) in %s\n";
	static const char *badinfo =
		"Warning: null m68k_serial for (%d, %d) in %s\n";

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
 * This is used to figure out the divisor speeds and the timeouts
 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 0 };

#define BAUD_TABLE_SIZE (sizeof(baud_table)/sizeof(baud_table[0]))

/* Sets or clears DTR/RTS on the requested line */
static inline void m68k_rtsdtr(struct m68k_serial *ss, int set)
{
	if (set) {
		/* set the RTS/CTS line */
	} else {
		/* clear it */
	}
	return;
}

/* Utility routines */
static inline int get_baud(struct m68k_serial *ss)
{
	unsigned long result = 115200;
	unsigned short int baud = UBAUD;
	if (GET_FIELD(baud, UBAUD_PRESCALER) == 0x38) result = 38400;
	result >>= GET_FIELD(baud, UBAUD_DIVIDE);

	return result;
}

/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void rs_stop(struct tty_struct *tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;
	
	save_flags(flags); cli();
	USTCNT &= ~USTCNT_TXEN;
	restore_flags(flags);
}

static void rs_put_char(char ch)
{
        int flags, loops = 0;

        save_flags(flags); cli();

	while (!(UTX & UTX_TX_AVAIL) && (loops < 1000)) {
        	loops++;
        	udelay(5);
        }

	UTX_TXDATA = ch;
        udelay(5);
        restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;
	
	save_flags(flags); cli();
	if (info->xmit_cnt && info->xmit_buf && !(USTCNT & USTCNT_TXEN)) {
#ifdef USE_INTS
		USTCNT |= USTCNT_TXEN | USTCNT_TX_INTR_MASK;
#else
		USTCNT |= USTCNT_TXEN;
#endif
	}
	restore_flags(flags);
}

/* Drop into either the boot monitor or kadb upon receiving a break
 * from keyboard/console input.
 */
static void batten_down_hatches(void)
{
	/* Drop into the debugger */
}

static _INLINE_ void status_handle(struct m68k_serial *info, unsigned short status)
{
#if 0
	if(status & DCD) {
		if((info->tty->termios->c_cflag & CRTSCTS) &&
		   ((info->curregs[3] & AUTO_ENAB)==0)) {
			info->curregs[3] |= AUTO_ENAB;
			info->pendregs[3] |= AUTO_ENAB;
			write_zsreg(info->m68k_channel, 3, info->curregs[3]);
		}
	} else {
		if((info->curregs[3] & AUTO_ENAB)) {
			info->curregs[3] &= ~AUTO_ENAB;
			info->pendregs[3] &= ~AUTO_ENAB;
			write_zsreg(info->m68k_channel, 3, info->curregs[3]);
		}
	}
#endif
	/* If this is console input and this is a
	 * 'break asserted' status change interrupt
	 * see if we can drop into the debugger
	 */
	if((status & URX_BREAK) && info->break_abort)
		batten_down_hatches();
	return;
}

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static _INLINE_ void rs_sched_event(struct m68k_serial *info,
				    int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void receive_chars(struct m68k_serial *info, struct pt_regs *regs, unsigned short rx)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;

	/*
	 * This do { } while() loop will get ALL chars out of Rx FIFO 
         */
#ifndef CONFIG_XCOPILOT_BUGS
	do {
#endif	
		ch = GET_FIELD(rx, URX_RXDATA);
	
		if(info->is_cons) {
			if(URX_BREAK & rx) { /* whee, break received */
				status_handle(info, rx);
				return;
			} else if (ch == 0x10) { /* ^P */
				show_state();
				show_free_areas();
				show_buffers();
/*				show_net_buffers(); */
				return;
			} else if (ch == 0x12) { /* ^R */
				machine_restart(NULL);
				return;
			}
			/* It is a 'keyboard interrupt' ;-) */
#ifdef CONFIG_CONSOLE
			wake_up(&keypress_wait);
#endif			
		}

		if(!tty)
			goto clear_and_exit;
		
		/*
		 * Make sure that we do not overflow the buffer
		 */
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			queue_task(&tty->flip.tqueue, &tq_timer);
			return;
		}

		if(rx & URX_PARITY_ERROR) {
			*tty->flip.flag_buf_ptr++ = TTY_PARITY;
			status_handle(info, rx);
		} else if(rx & URX_OVRUN) {
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
			status_handle(info, rx);
		} else if(rx & URX_FRAME_ERROR) {
			*tty->flip.flag_buf_ptr++ = TTY_FRAME;
			status_handle(info, rx);
		} else {
			*tty->flip.flag_buf_ptr++ = 0; /* XXX */
		}
                *tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;

#ifndef CONFIG_XCOPILOT_BUGS
	} while((rx = URX) & URX_DATA_READY);
#endif

	queue_task(&tty->flip.tqueue, &tq_timer);

clear_and_exit:
	return;
}

static _INLINE_ void transmit_chars(struct m68k_serial *info)
{
	if (info->x_char) {
		/* Send next char */
		UTX_TXDATA = info->x_char;
		info->x_char = 0;
		goto clear_and_return;
	}

	if((info->xmit_cnt <= 0) || info->tty->stopped) {
		/* That's peculiar... TX ints off */
		USTCNT &= ~USTCNT_TX_INTR_MASK;
		goto clear_and_return;
	}

	/* Send char */
	UTX_TXDATA = info->xmit_buf[info->xmit_tail++];
	info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
	info->xmit_cnt--;

	if (info->xmit_cnt < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

	if(info->xmit_cnt <= 0) {
		/* All done for now... TX ints off */
		USTCNT &= ~USTCNT_TX_INTR_MASK;
		goto clear_and_return;
	}

clear_and_return:
	/* Clear interrupt (should be auto)*/
	return;
}

/*
 * This is the serial driver's generic interrupt routine
 */
void rs_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct m68k_serial * info = &m68k_soft;
	unsigned short rx = URX;
#ifdef USE_INTS
	unsigned short tx = UTX;

	if (rx & URX_DATA_READY) receive_chars(info, regs, rx);
	if (tx & UTX_TX_AVAIL)   transmit_chars(info);
#else
	receive_chars(info, regs, rx);		
#endif
	return;
}

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct m68k_serial	*info = (struct m68k_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;
#if 0
	if (clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
#endif   
}

/*
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.  The path of
 * hangup processing is:
 *
 * 	serial interrupt routine -> (scheduler tqueue) ->
 * 	do_serial_hangup() -> tty->hangup() -> rs_hangup()
 * 
 */
static void do_serial_hangup(void *private_)
{
	struct m68k_serial	*info = (struct m68k_serial *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	tty_hangup(tty);
}


static int startup(struct m68k_serial * info)
{
	unsigned long flags;
	
	if (info->flags & S_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
		info->xmit_buf = (unsigned char *) get_free_page(GFP_KERNEL);
		if (!info->xmit_buf)
			return -ENOMEM;
	}

	save_flags(flags); cli();

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */

	USTCNT = USTCNT_UEN;
	info->xmit_fifo_size = 1;
	USTCNT = USTCNT_UEN | USTCNT_RXEN | USTCNT_TXEN;
	(void)URX;

	/*
	 * Finally, enable sequencing and interrupts
	 */
#ifdef USE_INTS
	USTCNT = USTCNT_UEN | USTCNT_RXEN | 
                 USTCNT_RX_INTR_MASK | USTCNT_TX_INTR_MASK;
#else
	USTCNT = USTCNT_UEN | USTCNT_RXEN | USTCNT_RX_INTR_MASK;
#endif

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * and set the speed of the serial port
	 */

	change_speed(info);

	info->flags |= S_INITIALIZED;
	restore_flags(flags);
	return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct m68k_serial * info)
{
	unsigned long	flags;

	USTCNT = 0; /* All off! */
	if (!(info->flags & S_INITIALIZED))
		return;

	save_flags(flags); cli(); /* Disable interrupts */
	
	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);
	
	info->flags &= ~S_INITIALIZED;
	restore_flags(flags);
}

struct {
	int divisor, prescale;
} hw_baud_table[18] = {
	{0,0}, /* 0 */
	{0,0}, /* 50 */
	{0,0}, /* 75 */
	{0,0}, /* 110 */
	{0,0}, /* 134 */
	{0,0}, /* 150 */
	{0,0}, /* 200 */
	{7,0x26}, /* 300 */
	{6,0x26}, /* 600 */
	{5,0x26}, /* 1200 */
	{0,0}, /* 1800 */
	{4,0x26}, /* 2400 */
	{3,0x26}, /* 4800 */
	{2,0x26}, /* 9600 */
	{1,0x26}, /* 19200 */
	{0,0x26}, /* 38400 */
	{1,0x38}, /* 57600 */
	{0,0x38}, /* 115200 */
};
/* rate = 1036800 / ((65 - prescale) * (1<<divider)) */

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct m68k_serial *info)
{
	unsigned short port;
	unsigned short ustcnt;
	unsigned cflag;
	int	i;

	if (!info->tty || !info->tty->termios)
		return;
	cflag = info->tty->termios->c_cflag;
	if (!(port = info->port))
		return;

	ustcnt = USTCNT;
	USTCNT = ustcnt & ~USTCNT_TXEN;

	i = cflag & CBAUD;
        if (i & CBAUDEX) {
                i = (i & ~CBAUDEX) + B38400;
        }

	info->baud = baud_table[i];
	UBAUD = PUT_FIELD(UBAUD_DIVIDE,    hw_baud_table[i].divisor) | 
		PUT_FIELD(UBAUD_PRESCALER, hw_baud_table[i].prescale);

	ustcnt &= ~(USTCNT_PARITYEN | USTCNT_ODD_EVEN | USTCNT_STOP | USTCNT_8_7);
	
	if ((cflag & CSIZE) == CS8)
		ustcnt |= USTCNT_8_7;
		
	if (cflag & CSTOPB)
		ustcnt |= USTCNT_STOP;

	if (cflag & PARENB)
		ustcnt |= USTCNT_PARITYEN;
	if (cflag & PARODD)
		ustcnt |= USTCNT_ODD_EVEN;
	
#ifdef CONFIG_68328_SERIAL_RTS_CTS
	if (cflag & CRTSCTS) {
		UTX &= ~ UTX_NOCTS;
	} else {
		UTX |= UTX_NOCTS;
	}
#endif

	ustcnt |= USTCNT_TXEN;
	
	USTCNT = ustcnt;
	return;
}

/*
 * Fair output driver allows a process to speak.
 */
static void rs_fair_output(void)
{
	int left;		/* Output no more than that */
	unsigned long flags;
	struct m68k_serial *info = &m68k_soft;
	char c;

	if (info == 0) return;
	if (info->xmit_buf == 0) return;

	save_flags(flags);  cli();
	left = info->xmit_cnt;
	while (left != 0) {
		c = info->xmit_buf[info->xmit_tail];
		info->xmit_tail = (info->xmit_tail+1) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
		restore_flags(flags);

		rs_put_char(c);

		save_flags(flags);  cli();
		left = MIN(info->xmit_cnt, left-1);
	}

	/* Last character is being transmitted now (hopefully). */
	udelay(5);

	restore_flags(flags);
	return;
}

/*
 * m68k_console_print is registered for printk.
 */
void console_print_68328(const char *p)
{
	char c;
	
	while((c=*(p++)) != 0) {
		if(c == '\n')
			rs_put_char('\r');
		rs_put_char(c);
	}

	/* Comment this if you want to have a strict interrupt-driven output */
	rs_fair_output();

	return;
}

static void rs_set_ldisc(struct tty_struct *tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_set_ldisc"))
		return;

	info->is_cons = (tty->termios->c_line == N_TTY);
	
	printk("ttyS%d console mode %s\n", info->line, info->is_cons ? "on" : "off");
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;
#ifndef USE_INTS
	for(;;) {
#endif
	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	/* Enable transmitter */
	save_flags(flags); cli();

#ifdef USE_INTS
	USTCNT |= USTCNT_TXEN | USTCNT_TX_INTR_MASK;
#else
	USTCNT |= USTCNT_TXEN;
#endif

#ifdef USE_INTS
	if (UTX & UTX_TX_AVAIL) {
#else
	if (1) {
#endif
		/* Send char */
		UTX_TXDATA = info->xmit_buf[info->xmit_tail++];
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt--;
	}

#ifndef USE_INTS
	while (!(UTX & UTX_TX_AVAIL)) udelay(5);
	}
#endif
	restore_flags(flags);
}

extern void console_printn(const char * b, int count);

static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, total = 0;
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty || !info->xmit_buf)
		return 0;

	save_flags(flags);
	while (1) {
		cli();		
		c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				   SERIAL_XMIT_SIZE - info->xmit_head));
		if (c <= 0)
			break;

		if (from_user) {
			down(&tmp_buf_sem);
			copy_from_user(tmp_buf, buf, c);
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, tmp_buf, c);
			up(&tmp_buf_sem);
		} else
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
		info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
		info->xmit_cnt += c;
		restore_flags(flags);
		buf += c;
		count -= c;
		total += c;
	}

	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
		/* Enable transmitter */
		cli();		
#ifndef USE_INTS
		while(info->xmit_cnt) {
#endif

		USTCNT |= USTCNT_TXEN;
#ifdef USE_INTS
		USTCNT |= USTCNT_TX_INTR_MASK;
#else
		while (!(UTX & UTX_TX_AVAIL)) udelay(5);
#endif
		if (UTX & UTX_TX_AVAIL) {
			UTX_TXDATA = info->xmit_buf[info->xmit_tail++];
			info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
			info->xmit_cnt--;
		}

#ifndef USE_INTS
		}
#endif
		restore_flags(flags);
	}
	restore_flags(flags);
	return total;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;
	int	ret;
				
	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return info->xmit_cnt;
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	sti();
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_throttle(struct tty_struct * tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		info->x_char = STOP_CHAR(tty);

	/* Turn off RTS line (do this atomic) */
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
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
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

static int get_serial_info(struct m68k_serial * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;
  
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->line;
	tmp.port = info->port;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	copy_to_user(retinfo,&tmp,sizeof(*retinfo));
	return 0;
}

static int set_serial_info(struct m68k_serial * info,
			   struct serial_struct * new_info)
{
	struct serial_struct new_serial;
	struct m68k_serial old_info;
	int 			retval = 0;

	if (!new_info)
		return -EFAULT;
	copy_from_user(&new_serial,new_info,sizeof(new_serial));
	old_info = *info;

	if (!suser()) {
		if ((new_serial.baud_base != info->baud_base) ||
		    (new_serial.type != info->type) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~S_USR_MASK) !=
		     (info->flags & ~S_USR_MASK)))
			return -EPERM;
		info->flags = ((info->flags & ~S_USR_MASK) |
			       (new_serial.flags & S_USR_MASK));
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
	info->flags = ((info->flags & ~S_FLAGS) |
			(new_serial.flags & S_FLAGS));
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
static int get_lsr_info(struct m68k_serial * info, unsigned int *value)
{
	unsigned char status;

	cli();
#ifdef CONFIG_68328_SERIAL_RTS_CTS
	status = (UTX & UTX_CTS_STAT) ? 1 : 0;
#else
	status = 0;
#endif
	sti();
	put_user(status,value);
	return 0;
}

/*
 * This routine sends a break character out the serial port.
 */
static void send_break(	struct m68k_serial * info, int duration)
{
        unsigned long flags;
        if (!info->port)
                return;
        current->state = TASK_INTERRUPTIBLE;
        save_flags(flags);
        cli();
#ifdef USE_INTS	
	UTX |= UTX_SEND_BREAK;
        schedule_timeout(duration);
	UTX &= ~UTX_SEND_BREAK;
#endif		
        restore_flags(flags);
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	int error;
	struct m68k_serial * info = (struct m68k_serial *)tty->driver_data;
	int retval;

	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
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
						sizeof(struct m68k_serial));
			if (error)
				return error;
			copy_to_user((struct m68k_serial *) arg,
				    info, sizeof(struct m68k_serial));
			return 0;
			
		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct m68k_serial *info = (struct m68k_serial *)tty->driver_data;

	if (tty->termios->c_cflag == old_termios->c_cflag)
		return;

	change_speed(info);

	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}
	
}

/*
 * ------------------------------------------------------------
 * rs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * S structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void rs_close(struct tty_struct *tty, struct file * filp)
{
	struct m68k_serial * info = (struct m68k_serial *)tty->driver_data;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
	
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("rs_close: bad serial port count for ttyS%d: %d\n",
		       info->line, info->count);
		info->count = 0;
	}
	if (info->count) {
		restore_flags(flags);
		return;
	}
	info->flags |= S_CLOSING;
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & S_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & S_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != S_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */

	USTCNT &= ~USTCNT_RXEN;
	USTCNT &= ~(USTCNT_RXEN | USTCNT_RX_INTR_MASK);

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
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(S_NORMAL_ACTIVE|S_CALLOUT_ACTIVE|
			 S_CLOSING);
	wake_up_interruptible(&info->close_wait);
	restore_flags(flags);
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void rs_hangup(struct tty_struct *tty)
{
	struct m68k_serial * info = (struct m68k_serial *)tty->driver_data;
	
	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;
	
	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(S_NORMAL_ACTIVE|S_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct m68k_serial *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	int		do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (info->flags & S_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & S_HUP_NOTIFY)
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
		if (info->flags & S_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & S_CALLOUT_ACTIVE) &&
		    (info->flags & S_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & S_CALLOUT_ACTIVE) &&
		    (info->flags & S_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= S_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & S_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= S_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & S_CALLOUT_ACTIVE) {
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
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);

	info->count--;
	info->blocked_open++;
	while (1) {
		cli();
		if (!(info->flags & S_CALLOUT_ACTIVE))
			m68k_rtsdtr(info, 1);
		sti();
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) ||
		    !(info->flags & S_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & S_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & S_CALLOUT_ACTIVE) &&
		    !(info->flags & S_CLOSING) && do_clocal)
			break;
                if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;

	if (retval)
		return retval;
	info->flags |= S_NORMAL_ACTIVE;
	return 0;
}	

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its S structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
int rs_open(struct tty_struct *tty, struct file * filp)
{
	struct m68k_serial	*info;
	int 			retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	
	if (line != 0) /* we have exactly one */
		return -ENODEV;

	info = &m68k_soft;

	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

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
		return retval;
	}

	if ((info->count == 1) && (info->flags & S_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else 
			*tty->termios = info->callout_termios;
		change_speed(info);
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

	return 0;
}

/* Finally, routines used to initialize the serial driver. */

static void show_serial_version(void)
{
	printk("MC68328 serial driver version 1.00\n");
}

volatile int test_done;

/* rs_init inits the driver */
static int __init
rs68328_init(void)
{
	int flags;
	struct m68k_serial *info;
	
	/* Setup base handler, and timer table. */
	init_bh(SERIAL_BH, do_serial_bh);
	show_serial_version();

	/* Initialize the tty_driver structure */
	/* SPARC: Not all of this is exactly right for us. */
	
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = 1;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag = 
			m68328_console_cbaud | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = rs_open;
	serial_driver.close = rs_close;
	serial_driver.write = rs_write;
	serial_driver.flush_chars = rs_flush_chars;
	serial_driver.write_room = rs_write_room;
	serial_driver.chars_in_buffer = rs_chars_in_buffer;
	serial_driver.flush_buffer = rs_flush_buffer;
	serial_driver.ioctl = rs_ioctl;
	serial_driver.throttle = rs_throttle;
	serial_driver.unthrottle = rs_unthrottle;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
	serial_driver.set_ldisc = rs_set_ldisc;

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
	callout_driver.name = "cua";
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
	save_flags(flags); cli();

	info = &m68k_soft;
	info->magic = SERIAL_MAGIC;
	info->port = 0xfffff900;
	info->tty = 0;
	info->irq = 0x40;
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
	info->callout_termios =callout_driver.init_termios;
	info->normal_termios = serial_driver.init_termios;
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	info->line = 0;
	info->is_cons = 1; /* Means shortcuts work */
	
	printk("%s%d at 0x%08x (irq = %d)", serial_driver.name, info->line, 
	       info->port, info->irq);
		printk(" is a builtin MC68328 UART\n");

        if (request_irq(UART_IRQ_NUM,
                        rs_interrupt,
                        IRQ_FLG_STD,
                        "M68328_UART", NULL))
                panic("Unable to attach 68328 serial interrupt\n");
	restore_flags(flags);
	return 0;
}

/*
 * register_serial and unregister_serial allows for serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
/* SPARC: Unused at this time, just here to make things link. */
int register_serial(struct serial_struct *req)
{
	return -1;
}

void unregister_serial(int line)
{
	return;
}
	
module_init(rs68328_init);
/* DAVIDM module_exit(rs68328_fini); */



static void m68328_set_baud()
{
	unsigned short ustcnt;
	int	i;

	ustcnt = USTCNT;
	USTCNT = ustcnt & ~USTCNT_TXEN;

again:
	for (i = 0; i < sizeof(baud_table) / sizeof(baud_table[0]); i++)
		if (baud_table[i] == m68328_console_baud)
			break;
	if (i >= sizeof(baud_table) / sizeof(baud_table[0])) {
		m68328_console_baud = 9600;
		goto again;
	}

	UBAUD = PUT_FIELD(UBAUD_DIVIDE,    hw_baud_table[i].divisor) | 
		PUT_FIELD(UBAUD_PRESCALER, hw_baud_table[i].prescale);
	ustcnt &= ~(USTCNT_PARITYEN | USTCNT_ODD_EVEN | USTCNT_STOP | USTCNT_8_7);
	ustcnt |= USTCNT_8_7;
	ustcnt |= USTCNT_TXEN;
	USTCNT = ustcnt;
	m68328_console_initted = 1;
	return;
}


int m68328_console_setup(struct console *cp, char *arg)
{
	int		i, n = CONSOLE_BAUD_RATE;

	if (!cp)
		return(-1);

	if (arg)
		n = simple_strtoul(arg,NULL,0);

	for (i = 0; i < BAUD_TABLE_SIZE; i++)
		if (baud_table[i] == n)
			break;
	if (i < BAUD_TABLE_SIZE) {
		m68328_console_baud = n;
		m68328_console_cbaud = 0;
		if (i > 15) {
			m68328_console_cbaud |= CBAUDEX;
			i -= 15;
		}
		m68328_console_cbaud |= i;
	}

	m68328_set_baud(); /* make sure baud rate changes */
	return(0);
}


static kdev_t m68328_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}


void m68328_console_write (struct console *co, const char *str,
			   unsigned int count)
{
	if (!m68328_console_initted)
		m68328_set_baud();
    while (count--) {
        if (*str == '\n')
           rs_put_char('\r');
        rs_put_char( *str++ );
    }
}


static struct console m68328_driver = {
	name:		"ttyS",
	write:		m68328_console_write,
	read:		NULL,
	device:		m68328_console_device,
	wait_key:	NULL,
	unblank:	NULL,
	setup:		m68328_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
	cflag:		0,
	next:		NULL
};


void m68328_console_init(void)
{
	register_console(&m68328_driver);
}

