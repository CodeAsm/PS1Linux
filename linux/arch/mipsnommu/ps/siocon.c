/*
 * Console over PlayStation SIO.
 */

#include <linux/tty.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/init.h>
#include <linux/console_struct.h>

#include <linux/console.h>
#include <linux/fs.h>

#include <asm/ps/sio.h>
#include <asm/types.h>
#include <asm/io.h>


#define SIO_NO_RESET
#define SIO_TIMEOUT
int sio_timeout;
typedef struct
{
	int mode;
	int ctrl;
	int rate;
} sio_regs_t;

void sio_con_udelay(int c)
{int j;
 for(j=0;j<c;j++);
}

const sio_regs_t sio_console_mode=
{
	mode: SIO_BRS16|SIO_CHR8|SIO_SB1,
	rate: SIO_B11520,
	ctrl: SIO_TX | SIO_RX | SIO_DTR | SIO_RTS
//	ctrl: SIO_TX|SIO_DTR|SIO_RTS|SIO_ACKIRQ
};

// save sio registers
void sio_save( sio_regs_t *sio )
{
	sio->rate = inw( SIO_RATE_REG );
	sio->mode = inw( SIO_MODE_REG );
	sio->ctrl = inw( SIO_CTRL_REG );	
}

// restore sio registers
void sio_restore( const sio_regs_t *sio )
{
// reset sio
#ifndef SIO_NO_RESET
	outw( SIO_RESET|SIO_ACKIRQ, SIO_CTRL_REG ); 
#endif // SIO_NO_RESET
	outw( sio->rate, SIO_RATE_REG );
	outw( sio->mode, SIO_MODE_REG );
	outw( sio->ctrl, SIO_CTRL_REG );	 
}

__volatile__ int sio_ready( int mask )
{
#ifdef SIO_TIMEOUT
  int cnt=0;
#endif // SIO_TIMEOUT
	while( (inw( SIO_STAT_REG ) & mask) != mask)
#ifdef SIO_TIMEOUT
		if(cnt++>9000) return -1;
#else
		;
#endif // SIO_TIMEOUT
  return 0;
}

static int  sio_getkey()
{
	int res, stat;
	sio_regs_t sio_cur_mode;

	sio_save( &sio_cur_mode );
#if 0
	sio_restore( &sio_console_mode );
#endif

	// raise RTS, disable interrupts
//	outw( (inw( SIO_CTRL_REG )&(~(SIO_ALLIRQ|SIO_BUF8))) | SIO_RTS, SIO_CTRL_REG );    
  
  // waiting for ready data 
	if( sio_ready(SIO_RFR)<0 ) 
		return -1;     
  res = inb( SIO_DATA_REG );

//	sio_restore( &sio_cur_mode );
  return res;

}

int sio_rtscts_get_8()
{
	sio_timeout=0;
	
	// raise RTS, disable interrupts
	outw( inw( SIO_CTRL_REG ) | SIO_RTS, SIO_CTRL_REG );    
  
  // waiting for data
	if( sio_ready(SIO_RFR)<0 ) 
	{
		sio_timeout=1;
		return -1;     
	}

	// drop RTS
	outw( inw( SIO_CTRL_REG ) & (~SIO_RTS), SIO_CTRL_REG );    

  return inb( SIO_DATA_REG );
}

// fill buffer from sio RTSCTS hardware control
// returns number of recieved bytes
int sio_rtscts_get_buf( __u8 *buf, const int length )
{
	int i;

	sio_timeout=0;
	if( (length ==0) || (buf ==NULL) ) return 0;  

	// raise RTS, disable interrupts
	outw( inw( SIO_CTRL_REG ) | SIO_RTS, SIO_CTRL_REG );    
  
  for( i=1; i<=length; i++, buf++ )
  {
  	// waiting for data
		if( sio_ready(SIO_RFR)<0 ) 
		{
			sio_timeout=1;
			return i-1;     
		}
		
		// drop RTS
		if( i==length )
			outw( inw( SIO_CTRL_REG ) & (~SIO_RTS), SIO_CTRL_REG );    

	  *buf = inb( SIO_DATA_REG );
	}
	return i-1;
}

// -- end of sio functions 



static void sio_console_write(struct console *co, const char *msgbuf,
			       unsigned size)
{
	int i;
	sio_regs_t sio_cur_mode;

	sio_save( &sio_cur_mode );
#if 0
	sio_restore( &sio_console_mode );
#endif

	// raise RTS, disable interrupts
//	outw( (inw( SIO_CTRL_REG )&(~(SIO_ALLIRQ|SIO_BUF8))) | SIO_RTS, SIO_CTRL_REG );    
//	outw( (inw( SIO_CTRL_REG )&(~(SIO_BUF8))) | SIO_RTS, SIO_CTRL_REG );    
	     // < size

  for (i = 0; i < size; i++) 
	{
    // waiting for ready to write 
		if( sio_ready(SIO_RFW)<0 ) 
			goto fend;
			     
      outb( msgbuf[i], SIO_DATA_REG );
		if (msgbuf[i] == 0x0a) {
			sio_ready(SIO_RFW); 
	    outb( 0x0d, SIO_DATA_REG );
	    
		}


	}   
fend:

}

static int sio_console_wait_key(struct console *co)
{int f;

 return sio_rtscts_get_8();

}

static void sio_console_read(struct console *co, const char *msgbuf,
			       unsigned size)
{
 int res;
 sio_rtscts_get_buf(msgbuf, size );

}

static int __init sio_console_setup(struct console *co, char *options)
{
  int  x,y;

   return 0;   // !!! check: we must return 0 on success ? !!! 
}

static kdev_t sio_console_device(struct console *c)
{
    return MKDEV(TTY_MAJOR, 64 + c->index);
}

static struct console siocons =
{
    name:	"ttyS",
    write:	sio_console_write,
    read:	sio_console_read,
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

    outw(0x50, SIO_CTRL_REG );
    sio_con_udelay(1500);
    outw(SIO_B11520, SIO_RATE_REG );
    sio_con_udelay(1500);
    outw(SIO_BRS16|SIO_CHR8|SIO_SB1, SIO_MODE_REG );
    sio_con_udelay(1500);
    outw(0x5, SIO_CTRL_REG );
    sio_con_udelay(1500);
    outw(inw(SIO_CTRL_REG)|SIO_RXIRQ|SIO_RX|SIO_TX, SIO_CTRL_REG );
    sio_con_udelay(1500);
        
    printk("PSX SIO console enable \n");
    register_console(&siocons);
}
