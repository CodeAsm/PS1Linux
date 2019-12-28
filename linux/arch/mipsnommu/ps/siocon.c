/*
 * Wrap-around code for a console on the
 * PlayStation SIO.
 */

#include <linux/tty.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/fs.h>
#include <asm/ps/sio.h>

static void sio_console_write(struct console *co, const char *s,
			       unsigned count)
{
   int i;
   
   ps_sio_set_control (ps_sio_get_control () | SIO_RTS);
   
   for (i = 0; i < count; i++) {
      /* waiting for ready to write */
      while (!(ps_sio_get_status () & SIO_RFW)) ;
      ps_sio_set_byte (s[i]);
   }
   
   ps_sio_set_control (ps_sio_get_control () & (~SIO_RTS));
}

static int sio_console_wait_key(struct console *co)
{
   int res;

   ps_sio_set_control (ps_sio_get_control () | SIO_RTS);
   /* waiting for ready data */
   while (!(ps_sio_get_status () & SIO_RFR)) ;
   res = ps_sio_get_byte ();
   ps_sio_set_control (ps_sio_get_control () & (~SIO_RTS));
   
   return res;
}

static int __init sio_console_setup(struct console *co, char *options)
{
   /* set sio mode: 8 bits data, prescaler 16,	no parity, 1 stop bit */
   ps_sio_set_mode (SIO_CHR8 | SIO_BRS16 | SIO_SB1);
   /* set baudrate: 115200 */
   ps_sio_set_baundrate (0x1c200);
   /* enable Tx, Rx, disable interrupts */
   ps_sio_set_control (SIO_TX | SIO_RX);
   return 0;   /* !!! check: we must return 0 on success ? !!! */
}

static kdev_t sio_console_device(struct console *c)
{
    return MKDEV(TTY_MAJOR, 64 + c->index);
}

static struct console sercons =
{
    name:	"ttyS",
    write:	sio_console_write,
    device:	sio_console_device,
    wait_key:	sio_console_wait_key,
    setup:	sio_console_setup,
    flags:	CON_PRINTBUFFER,     /* !!! ??? !!! */
    index:	-1,
};

/*
 *    Register console.
 */

void __init ps_sio_console_init(void)
{
    register_console(&sercons);
}
