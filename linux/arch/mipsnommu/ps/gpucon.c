/*
 * Console over PlayStation GPU.
 */

#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/init.h>
#include <linux/console_struct.h>

#include <linux/console.h>
#include <linux/fs.h>

//#include <asm/ps/sio.h>
//#include <asm/types.h>
#include <asm/io.h>
#include <asm/ps/libpsx.h>

//  MY DEFS & VARS
#define PSX_SCR_H	21
#define PSX_SCR_W	36
#define PSX_VSCR_W	37
#define PSX_FNT_H	12
#define PSX_FNT_W	 8

static char psx_scrbuf[PSX_VSCR_W*PSX_SCR_H+1];		//PSX TEXT SCREEN BUFFER
static char psx_curstr;															// POINTER TO CURRENT STRING
static char psx_curx;															// POINTER TO CURRENT POSITION IN STR
// END OF DEFS & VARS


//??? Standart rotines

static void gpu_console_write(struct console *co, const char *msgbuf,
			       unsigned size)
{
	int x,y,z;
	

// GPU section
	if(size!=65535)z=size;else z=65534;

	for(x=0;x<z;x++)
	{
	if(msgbuf[x]==0)x=65535;
	else {
		if(msgbuf[x]==10)				// NEW LINE
		{	psx_curx=0;
			psx_curstr++;
			if (psx_curstr==PSX_SCR_H)psx_curstr=0;
			for(y=0;y<PSX_SCR_W;y++)		//clear string
			 psx_scrbuf[y+psx_curstr*PSX_VSCR_W]=0;		//
		}
		else
		if(msgbuf[x]==13)psx_curx=0;				// BEGIN STR
		else
		if(psx_curx<PSX_SCR_W){psx_scrbuf[psx_curx+psx_curstr*PSX_VSCR_W]=msgbuf[x];psx_curx++;}
		else {  psx_curx=0;
			psx_curstr++;
			if (psx_curstr==PSX_SCR_H)psx_curstr=0;
			for(y=psx_curx;y<PSX_SCR_W;y++)		//clear string
			 psx_scrbuf[y+psx_curstr*PSX_VSCR_W]=0;		//
			psx_scrbuf[psx_curx+psx_curstr*PSX_VSCR_W]=msgbuf[x];	
			psx_curx++;
			}
		}
	    } // str->buf cycle end
	cls();
	z=0;
	
	for(y=psx_curstr+1;y<PSX_SCR_H;y++) // print screen after cursor string
	{for(x=0;x<PSX_SCR_W;x++)
	{
	if(psx_scrbuf[x+y*PSX_VSCR_W]!=0)
	if(psx_scrbuf[x+y*PSX_VSCR_W]!=' ')
	{
	print2(x*PSX_FNT_W,z*PSX_FNT_H,psx_scrbuf[x+y*PSX_VSCR_W]);
	gpu_dma_gpu_idle();
	}
	}z++;}

	for(y=0;y<psx_curstr+1;y++) // print screen before cursor string
	{for(x=0;x<PSX_SCR_W;x++)
	{
	if(psx_scrbuf[x+y*PSX_VSCR_W]!=0)
	if(psx_scrbuf[x+y*PSX_VSCR_W]!=' ')
	{
	print2(x*PSX_FNT_W,z*PSX_FNT_H,psx_scrbuf[x+y*PSX_VSCR_W]);
	gpu_dma_gpu_idle();
	}
	}
	z++;
	}

}

static int gpu_console_wait_key(struct console *co)
{
// stub
   return 0;
}


static int __init gpu_console_setup(struct console *co, char *options)
{
  int  x,y;

 for (x=0;x<PSX_VSCR_W*PSX_SCR_H;x++)
     psx_scrbuf[x]=0;		
     psx_curstr=0;	
     psx_curx=0;

	InitGPU( 0x8000009 );
	cls();
	LoadFont();


   return 0;   /* !!! check: we must return 0 on success ? !!! */
}

static kdev_t gpu_console_device(struct console *c)
{
    return MKDEV(TTY_MAJOR, 64 + c->index);
}

static struct console sercons =
{
    name:	"ttyS",
    write:	gpu_console_write,
    device:	gpu_console_device,
    wait_key:	gpu_console_wait_key,
    setup:	gpu_console_setup,
    flags:	CON_PRINTBUFFER,     /* !!! ??? !!! */
    index:	-1,
};

/*
 *    Register console.
 */

void __init ps_gpu_console_init(void)
{
    
    register_console(&sercons);
}
