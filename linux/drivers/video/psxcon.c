/*
 *	Virtual Console over PlayStation GPU.
 * FIXME: Virtual screen resolution is not supported now !!!
 */

#undef FBCONDEBUG

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>	/* MSch: for IRQ probe */
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/console_struct.h>
#include <linux/string.h>
#include <linux/kd.h>
#include <linux/malloc.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>
#include <linux/smp.h>
#include <linux/init.h>

#include <asm/ps/libpsx.h>

//#include <asm/irq.h>
//#include <asm/system.h>
//#include <asm/uaccess.h>
//#define INCLUDE_LINUX_LOGO_DATA
//#include <asm/linux_logo.h>

//#include <video/fbcon.h>
//#include <video/fbcon-mac.h>	/* for 6x11 font on mac */
//#include <video/font.h>

//#ifdef FBCONDEBUG
//#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
//#else
//#  define DPRINTK(fmt, args...)
//#endif


//  MY DEFS & VARS
#define PSXVGA_SCR_H	21
#define PSXVGA_SCR_W	37
#define PSXVGA_VSCR_H	(PSXVGA_SCR_H)
#define PSXVGA_VSCR_W	(PSXVGA_SCR_W)
#define PSXVGA_FNT_H	12
#define PSXVGA_FNT_W	 8

static unsigned int psxvga_scrbuf[PSXVGA_VSCR_H][PSXVGA_VSCR_W];  //PSX TEXT SCREEN BUFFER
static char psxvga_cury;				// POINTER TO CURRENT STRING
static char psxvga_curx;				// POINTER TO CURRENT POSITION IN STR
static int psxvga_bottom;
// END OF THEM


/*
 Functions needed by some files (keyboard.c pc_keyb and etc) for keyboard support.
*/

struct kbd_ops {
	/* Keyboard driver resource allocation  */
	void (*kbd_request_region)(void);
	int (*kbd_request_irq)(void (*handler)(int, void *, struct pt_regs *));

	/* PSaux driver resource managment  */
	int (*aux_request_irq)(void (*handler)(int, void *, struct pt_regs *));
	void (*aux_free_irq)(void);

	/* Methods to access the keyboard processor's I/O registers  */
	unsigned char (*kbd_read_input)(void);
	void (*kbd_write_output)(unsigned char val);
	void (*kbd_write_command)(unsigned char val);
	unsigned char (*kbd_read_status)(void);
};

 int kbd_setkeycode(unsigned int scancode,unsigned int keycode){return 0;};
 int kbd_getkeycode(unsigned int scancode){return 0;};
 int kbd_translate(unsigned char keycode, unsigned char *keycodep,
		     char raw_mode){return 0;};
 int kbd_unexpected_up(unsigned char keycode){return 0;};
 void kbd_leds(unsigned char leds){};
 void kbd_init_hw(void){ };
 struct kbd_ops no_kbd_ops;
/*
 *  Interface used by the world
 */

static const char *psxvga_startup(void);
static void psxvga_init(struct vc_data *conp, int init);
static void psxvga_deinit(struct vc_data *conp);
static void psxvga_clear(struct vc_data *conp, int sy, int sx, int height,
		       int width);
static void psxvga_putc(struct vc_data *conp, int c, int ypos, int xpos);
static void psxvga_putcs(struct vc_data *conp, const unsigned short *s, int count,
			int ypos, int xpos);
static void psxvga_cursor(struct vc_data *conp, int mode);
static int  psxvga_scroll(struct vc_data *conp, int t, int b, int dir,
			 int count);
static void psxvga_bmove(struct vc_data *conp, int sy, int sx, int dy, int dx,
			int height, int width);
static int  psxvga_switch(struct vc_data *conp);
static int  psxvga_blank(struct vc_data *conp, int blank);
static int  psxvga_font_op(struct vc_data *conp, struct console_font_op *op);
static int  psxvga_set_palette(struct vc_data *conp, unsigned char *table);
static int  psxvga_scrolldelta(struct vc_data *conp, int lines);


/*
 *  Low Level Operations
 */


static const char *psxvga_startup (void)
{
   const char *display_desc = "PSXGPU console";
   int  x, y;

   for (y = 0; y < PSXVGA_VSCR_H; y++)
      for (x = 0; x < PSXVGA_VSCR_W; x++)
         psxvga_scrbuf[y][x] = 0;
         
   psxvga_cury = 0;	
   psxvga_curx = 0;

   InitGPU (0x8000009);
   cls ();
   LoadFont ();
   
	psxvga_bottom = 0;

   return  display_desc;
}


static void psxvga_init(struct vc_data *conp, int init)
{

	conp->vc_can_do_color = 0;
	conp->vc_cols = PSXVGA_VSCR_W;
	conp->vc_rows = PSXVGA_VSCR_H;
	conp->vc_x=(unsigned int)psxvga_curx;
	conp->vc_y=(unsigned int)psxvga_cury;
	conp->vc_origin=(unsigned long)0;
	conp->vc_complement_mask=0x7700;
	conp->vc_size_row=PSXVGA_VSCR_W*2;
}


static void psxvga_deinit(struct vc_data *conp)
{

}

static inline void psxvga_writew2 (unsigned int val, int y, int x)
{
   if (y < PSXVGA_VSCR_H && x < PSXVGA_VSCR_W)
   {
	   print2 (x*PSXVGA_FNT_W, y*PSXVGA_FNT_H, val);  // !!! need additional test 
	   gpu_dma_gpu_idle();                            // for virtual screen resolution !!!
      
      y += psxvga_bottom;
      if (y >= PSXVGA_VSCR_H) y -= PSXVGA_VSCR_H;
      psxvga_scrbuf[y][x] = val;
   }
}

static inline void psxvga_printscreen (void)
{
   int x, y, z;
   
	cls ();
	
	for (y = 0, z = psxvga_bottom; z < PSXVGA_SCR_H; z++, y++) // print top piece of screen
	{
      for(x = 0; x < PSXVGA_SCR_W; x++)
	   {
	      if (psxvga_scrbuf[z][x] != 0 && psxvga_scrbuf[z][x] != ' ')
	      {
	         print2 (x*PSXVGA_FNT_W, y*PSXVGA_FNT_H, psxvga_scrbuf[z][x]);
	         gpu_dma_gpu_idle();
	      }
	   }
   }

	for (z = 0; z < psxvga_bottom; z++, y++) // print bottom piece of screen
	{
      for (x = 0; x < PSXVGA_SCR_W; x++)
	   {
	      if (psxvga_scrbuf[z][x] != 0 && psxvga_scrbuf[z][x] != ' ')
	      {
	         print2 (x*PSXVGA_FNT_W, y*PSXVGA_FNT_H, psxvga_scrbuf[z][x]);
	         gpu_dma_gpu_idle();
	      }
	   }
	}
}


static inline u16 psxvga_readw (u16 addr)
{
   int x, y;
   
   y = addr/PSXVGA_VSCR_W;
   x = addr-y*PSXVGA_VSCR_W;

   return (u16)psxvga_scrbuf[y][x];
}

static inline void psxvga_memsetw(u16 sx,u16 sy, u16 c, unsigned int count)
{
   while (count) {
	   count--;
	   psxvga_writew2 (c, sy,sx);
   }
}


static inline void psxvga_memmovew(u16 to, u16 from, int count)
{
/*
    if (to < from) {
	while (count) {
	    count--;
	    psxvga_writew(psxvga_readw(from++), to++);
	}
    } else {
	from += count;
	to += count;
	while (count) {
	    count--;
	    psxvga_writew(psxvga_readw(--from), --to);
	}
    }
*/
}

/* ====================================================================== */

/*  fbcon_XXX routines - interface used by the world
 *
 *  This system is now divided into two levels because of complications
 *  caused by hardware scrolling. Top level functions:
 *
 *	fbcon_bmove(), fbcon_clear(), fbcon_putc()
 *
 *  handles y values in range [0, scr_height-1] that correspond to real
 *  screen positions. y_wrap shift means that first line of bitmap may be
 *  anywhere on this display. These functions convert lineoffsets to
 *  bitmap offsets and deal with the wrap-around case by splitting blits.
 *
 *	fbcon_bmove_physical_8()    -- These functions fast implementations
 *	fbcon_clear_physical_8()    -- of original fbcon_XXX fns.
 *	fbcon_putc_physical_8()	    -- (fontwidth != 8) may be added later
 *
 *  WARNING:
 *
 *  At the moment fbcon_putc() cannot blit across vertical wrap boundary
 *  Implies should only really hardware scroll in rows. Only reason for
 *  restriction is simplicity & efficiency at the moment.
 */

static void psxvga_clear(struct vc_data *conp, int sy, int sx, int height,
			int width)
{
/*
int y;
 if((sy>PSXVGA_VSCR_H)){}
 else
  {
 for(y=0;y<height;y++)
  psxvga_memsetw(sx,(sy+y)*PSXVGA_VSCR_W,' ', width);
  }
  */
}


static void psxvga_putc(struct vc_data *conp, int c, int ypos, int xpos)
{
   psxvga_writew2 (c, ypos, xpos);
}


static void psxvga_putcs(struct vc_data *conp, const unsigned short * s, int count,
		       int ypos, int xpos)
{
   int i;

   for (i = 0; i < count; i++)
	{
      psxvga_writew2 ((scr_readw(s++)), ypos, xpos+i);
   }
}


static void psxvga_cursor(struct vc_data *conp, int mode)
{

}



static int psxvga_scroll(struct vc_data *conp, int t, int b, int dir, int count)
{
   int x, y, i;
   
   switch (dir)
   {
      case SM_UP:
		   for (y = psxvga_bottom, i = 0; i < count; i++, y++) {
            if (y >= PSXVGA_VSCR_H) y = 0;
		      for (x = 0; x < PSXVGA_VSCR_W; x++)
		         psxvga_scrbuf[y][x] = 0;
         }      
         psxvga_bottom += count;
	      if (psxvga_bottom >= PSXVGA_VSCR_H)
            psxvga_bottom -= PSXVGA_VSCR_H;
            
         break;
         
      case SM_DOWN:
//       memcpy(&psxvga_scrbuf[count*PSXVGA_VSCR_W],&psxvga_scrbuf[0],PSXVGA_VSCR_W*PSXVGA_VSCR_H-count*PSXVGA_VSCR_W);
//       memset(&psxvga_scrbuf[PSXVGA_VSCR_W*PSXVGA_VSCR_H-count*PSXVGA_VSCR_W],0,count*PSXVGA_VSCR_W);
         break;
   }
   
	psxvga_printscreen ();    
   return 0;
}


static void psxvga_bmove(struct vc_data *conp, int sy, int sx, int dy, int dx,
			int height, int width)
{
/*
   int y;
 if((sy>PSXVGA_VSCR_H)&&(dy>PSXVGA_VSCR_H)){}
 else
  {
 for(y=0;y<height;y++)
  psxvga_memmovew(sx+(sy+y)*PSXVGA_VSCR_W,dx+(dy+y)*PSXVGA_VSCR_W , width);
  }
*/
}


static int psxvga_switch(struct vc_data *conp)
{
    return 1;
}


static int psxvga_blank(struct vc_data *conp, int blank)
{
    return 0;
}


static int psxvga_font_op(struct vc_data *conp, struct console_font_op *op)
{
	    return -ENOSYS;
}

static int psxvga_set_palette(struct vc_data *conp, unsigned char *table)
{
 return -EINVAL;
}

static u16 *psxvga_screen_pos(struct vc_data *conp, int offset)
{
    return (u16 *)(psxvga_cury+offset);
}

static unsigned long psxvga_getxy(struct vc_data *conp, unsigned long pos, int *px, int *py)
{unsigned long ret;
    
    if (px) *px = psxvga_curx;
    if (py) *py = psxvga_cury;
    ret = pos + (PSXVGA_VSCR_W - psxvga_curx) * 2; // WARNING !!!!
    
    return ret;
}

static void psxvga_invert_region(struct vc_data *conp, u16 *p, int cnt)
{

}

static int psxvga_scrolldelta(struct vc_data *conp, int lines)
{
    return 0;
}

static int psxvga_set_origin(struct vc_data *conp)
{
    return 0;
}

static void psxvga_save_screen(struct vc_data *conp)
{

}
static u8 psxvga_build_attr(struct vc_data *conp,u8 color, u8 intens, u8 blink, u8 underline, u8 reverse)
{
  u8 attr = color;
 return attr;
}

/*
 *  The console `switch' structure for the PSX GPU based console
 */
 
const struct consw psxvga_con = {
    con_startup: 	psxvga_startup, 
    con_init: 		psxvga_init,
    con_deinit: 	psxvga_deinit,
    con_clear: 		psxvga_clear,
    con_putc: 		psxvga_putc,
    con_putcs: 		psxvga_putcs,
    con_cursor: 	psxvga_cursor,
    con_scroll: 	psxvga_scroll,
    con_bmove: 		psxvga_bmove,
    con_switch: 	psxvga_switch,
    con_blank: 		psxvga_blank,
    con_font_op:	psxvga_font_op,
    con_set_palette: 	psxvga_set_palette,
    con_scrolldelta: 	psxvga_scrolldelta,
    con_set_origin: 	psxvga_set_origin,
    con_save_screen:	psxvga_save_screen,
    con_build_attr:	psxvga_build_attr,
    con_invert_region:	psxvga_invert_region,
    con_screen_pos:	psxvga_screen_pos,
    con_getxy:		psxvga_getxy
};


/*
 *  Dummy Low Level Operations
 */

static void psxvga_dummy_op(void) {}

#define DUMMY	(void *)psxvga_dummy_op
/*
struct display_switch psxvga_dummy = {
    setup:	DUMMY,
    bmove:	DUMMY,
    clear:	DUMMY,
    putc:	DUMMY,
    putcs:	DUMMY,
    revc:	DUMMY,
};
*/

/*
 *  Visible symbols for modules
 */

EXPORT_SYMBOL(psxvga_redraw_bmove);
EXPORT_SYMBOL(psxvga_redraw_clear);
EXPORT_SYMBOL(psxvga_dummy);
EXPORT_SYMBOL(psxvga_con);



