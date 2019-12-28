/*
 * linux/drivers/char/serial_psx.c
 *
 * Driver for the serial port on the PSX.
 *
 * Based on drivers/char/serial.c
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/console.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include <asm/ps/sio.h>
#include <asm/ps/hwregs.h>
#include <asm/types.h>
#include <asm/ps/interrupts.h>

#define BAUD_BASE		(mem_fclk_21285/64)

#define SERIAL_PSX_NAME	"ttyFB"
//#define SERIAL_PSX_MAJOR	204
#define SERIAL_PSX_MINOR	64
#define SERIAL_PSX_MAJOR	TTY_MAJOR
//#define SERIAL_PSX_MINOR	(64 + c->index)

#define SERIAL_PSX_AUXNAME	"cuafb"
#define SERIAL_PSX_AUXMAJOR	205
#define SERIAL_PSX_AUXMINOR	4

static struct tty_driver psxs_driver, callout_driver;
static int psxs_refcount;
static struct tty_struct *psxs_table[1];

static struct termios *psxs_termios[1];
static struct termios *psxs_termios_locked[1];

static char wbuf[1000], *putp = wbuf, *getp = wbuf, x_char;
static struct tty_struct *psxs_tty;
static int psxs_use_count;


static int psxs_write_room(struct tty_struct *tty)
{
	return putp >= getp ? (sizeof(wbuf) - (long) putp + (long) getp) : ((long) getp - (long) putp - 1);
}

static void psxs_rx_int(int irq, void *dev_id, struct pt_regs *regs)
{int  flag;int i;
 int ch;
 long x;

	if (!psxs_tty) {
		return;
	}

  ch=inb(SIO_DATA_REG);
    flag=0;
		
		if (inw(SIO_STAT_REG) & SIO_OERR)
			tty_insert_flip_char(psxs_tty, 0, TTY_OVERRUN);
		if (inw(SIO_STAT_REG) & SIO_PERR)
			flag = TTY_PARITY;
		else if (inw(SIO_STAT_REG) & SIO_FERR)
			flag = TTY_FRAME;

		tty_insert_flip_char(psxs_tty, ch, flag);

		tty_flip_buffer_push(psxs_tty);
//		tty_schedule_flip(psxs_tty);


	


 outw((inw(SIO_CTRL_REG)|SIO_ACKIRQ|SIO_RXIRQ),SIO_CTRL_REG);

}

static void psxs_send_xchar(struct tty_struct *tty, char ch)
{
	x_char = ch;
}

static void psxs_throttle(struct tty_struct *tty)
{
	if (I_IXOFF(tty))
		psxs_send_xchar(tty, STOP_CHAR(tty));
}

static void psxs_unthrottle(struct tty_struct *tty)
{
	if (I_IXOFF(tty)) {
		if (x_char)
			x_char = 0;
		else
			psxs_send_xchar(tty, START_CHAR(tty));
	}
}



static inline int psxs_xmit(int ch)
{int i,size=1;
	 size=1;

  for (i = 0; i < size; i++) 
	{
    // waiting for ready to write 
		if( sio_ready(SIO_RFW)<0 ) 
		
			goto psxfend;
			     
      outb( ch, SIO_DATA_REG );
		if (ch == 0x0a) {
			sio_ready(SIO_RFW); 
	    outb( 0x0d, SIO_DATA_REG );
	    
		}


	}   
psxfend:



	return 1;
	
}

static int psxs_write(struct tty_struct *tty, int from_user,
		       const u_char * buf, int count)
{
	int i;

	if (from_user && verify_area(VERIFY_READ, buf, count))
		return -EINVAL;

	for (i = 0; i < count; i++) {
		char ch;
		if (from_user)
			__get_user(ch, buf + i);
		else
			ch = buf[i];
		if (!psxs_xmit(ch))
			break;
	}
	return i;
}

static void psxs_put_char(struct tty_struct *tty, u_char ch)
{
	psxs_xmit(ch);
}

static int psxs_chars_in_buffer(struct tty_struct *tty)
{
	return sizeof(wbuf) - psxs_write_room(tty);
}

static void psxs_flush_buffer(struct tty_struct *tty)
{
//	disable_irq(SIO);
	putp = getp = wbuf;
//	if (x_char)enable_irq(SIO);
}

static inline void psxs_set_cflag(int cflag)
{
	int h_lcr, baud, quot;
/*
	switch (cflag & CSIZE) {
	case CS5:
		h_lcr = 0x10;
		break;
	case CS6:
		h_lcr = 0x30;
		break;
	case CS7:
		h_lcr = 0x50;
		break;
	default:
		h_lcr = 0x70;
		break;

	}
	if (cflag & CSTOPB)
		h_lcr |= 0x08;
	if (cflag & PARENB)
		h_lcr |= 0x02;
	if (!(cflag & PARODD))
		h_lcr |= 0x04;
*/
	switch (cflag & CBAUD) {
	case B115200:	baud = 115200;		break;
	default:
	case B57600:	baud = 57600;		break;
	}
/*
	quot = (BAUD_BASE - (baud >> 1)) / baud;
	*CSR_UARTCON = 0;
	*CSR_L_UBRLCR = quot & 0xff;
	*CSR_M_UBRLCR = (quot >> 8) & 0x0f;
	*CSR_H_UBRLCR = h_lcr;
	*CSR_UARTCON = 1;
*/
}

static void psxs_set_termios(struct tty_struct *tty, struct termios *old)
{
	if (old && tty->termios->c_cflag == old->c_cflag)
		return;
	psxs_set_cflag(tty->termios->c_cflag);

}


static void psxs_stop(struct tty_struct *tty)
{
//disable_irq(SIO);
}

static void psxs_start(struct tty_struct *tty)
{
//enable_irq(SIO);
}

static void psxs_wait_until_sent(struct tty_struct *tty, int timeout)
{
//	int orig_jiffies = jiffies;

//	while (*CSR_UARTFLG & 8) {
//		current->state = TASK_INTERRUPTIBLE;
//		schedule_timeout(1);
//		if (signal_pending(current))
//			break;
//		if (timeout && time_after(jiffies, orig_jiffies + timeout))
//			break;
//	}

//	current->state = TASK_RUNNING;

}

static int psxs_open(struct tty_struct *tty, struct file *filp)
{
	int line;

	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if (line){
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}

	tty->driver_data = NULL;
	if (!psxs_tty)
		psxs_tty = tty;

//	enable_irq(SIO);
	psxs_use_count++;

	return 0;
}

static void psxs_close(struct tty_struct *tty, struct file *filp)
{
	if (!--psxs_use_count) {
		psxs_wait_until_sent(tty, 0);
//		disable_irq(SIO);
		psxs_tty = NULL;
	}
	MOD_DEC_USE_COUNT;
	
}

static int __init psxs_init(void)
{int x;
	int baud = B115200;

	
	psxs_driver.magic = TTY_DRIVER_MAGIC;
	psxs_driver.driver_name = "serial";
	psxs_driver.name = "ttyS";
	psxs_driver.major = SERIAL_PSX_MAJOR;
	psxs_driver.minor_start = SERIAL_PSX_MINOR;
	psxs_driver.num = 1;
	psxs_driver.type = TTY_DRIVER_TYPE_SERIAL;
	psxs_driver.subtype = SERIAL_TYPE_NORMAL;
	psxs_driver.init_termios = tty_std_termios;
	psxs_driver.init_termios.c_cflag = baud | CS8 | CREAD | HUPCL | CLOCAL;
	psxs_driver.flags = TTY_DRIVER_REAL_RAW|TTY_DRIVER_NO_DEVFS;
	psxs_driver.refcount = &psxs_refcount;
	psxs_driver.table = psxs_table;
	psxs_driver.termios = psxs_termios;
	psxs_driver.termios_locked = psxs_termios_locked;

	psxs_driver.open = psxs_open;
	psxs_driver.close = psxs_close;
	psxs_driver.write = psxs_write;
	psxs_driver.put_char = psxs_put_char;
	psxs_driver.write_room = psxs_write_room;
	psxs_driver.chars_in_buffer = psxs_chars_in_buffer;
	psxs_driver.flush_buffer = psxs_flush_buffer;
	psxs_driver.throttle = psxs_throttle;
	psxs_driver.unthrottle = psxs_unthrottle;
	psxs_driver.send_xchar = psxs_send_xchar;
	psxs_driver.set_termios = psxs_set_termios;
	psxs_driver.stop = psxs_stop;
	psxs_driver.start = psxs_start;
	psxs_driver.wait_until_sent = psxs_wait_until_sent;

	callout_driver = psxs_driver;
	callout_driver.name = "cua";
	callout_driver.major = SERIAL_PSX_AUXMAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	outw(0x50,SIO_CTRL_REG);
	for(x=0;x<1500;x++);
	outw(SIO_B11520,SIO_RATE_REG);
	for(x=0;x<1500;x++);
	outw(SIO_BRS16|SIO_CHR8|SIO_SB1,SIO_MODE_REG);
	for(x=0;x<1500;x++);
	outw(SIO_RX|SIO_TX,SIO_CTRL_REG);
	for(x=0;x<15000;x++);
	
		
	if (request_irq(SIO, psxs_rx_int, /*SA_INTERRUPT*/0, "serial", NULL))
		panic("Couldn't get irq for PSX serial port");

	if (tty_register_driver(&psxs_driver))
		printk(KERN_ERR "Couldn't register PSX serial driver\n");
	if (tty_register_driver(&callout_driver))
		printk(KERN_ERR "Couldn't register PSX callout driver\n");
	
	tty_register_devfs(&psxs_driver,0,psxs_driver.minor_start+0);
	tty_register_devfs(&callout_driver,0,psxs_driver.minor_start+0);
	
	for(x=0;x<15000;x++);
	outw(inw(SIO_CTRL_REG)|SIO_RXIRQ|SIO_RX|SIO_TX|SIO_ACKIRQ|SIO_RTS,SIO_CTRL_REG);
	for(x=0;x<15000;x++);
	printk("PSX serial port driver\n");
	return 0;
}

int __init psxs_init_tty(void)
{
 return psxs_init();
  
}

static void __exit psxs_fini(void)
{
	unsigned long flags;
	int ret;

	save_flags(flags);
	cli();
	ret = tty_unregister_driver(&callout_driver);
	if (ret)
		printk(KERN_ERR "Unable to unregister PSX_SIO callout driver "
			"(%d)\n", ret);
	ret = tty_unregister_driver(&psxs_driver);
	if (ret)
		printk(KERN_ERR "Unable to unregister PSX_SIO driver (%d)\n",
			ret);
//	free_irq(SIO, NULL);
	restore_flags(flags);
	
}

module_init(psxs_init);
module_exit(psxs_fini);


