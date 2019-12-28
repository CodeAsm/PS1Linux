/*
 * linux/arch/m68knommu/console/68328fb.c -- Low level implementation of the
 *                                           mc68328 LCD frame buffer device
 *
 *    Copyright (C) 1998,1999 Kenneth Albanowski <kjahds@kjahds.com>,
 *                            The Silver Hammer Group, Ltd.
 *
 *
 * This file is based on the Amiga CyberVision frame buffer device (Cyberfb.c):
 *
 *    Copyright (C) 1996 Martin Apel
 *                       Geert Uytterhoeven
 *
 *
 * This file is based on the Amiga frame buffer device (amifb.c):
 *
 *    Copyright (C) 1995 Geert Uytterhoeven
 *
 *
 * History:
 *   - 17 Feb 98: Original version by Kenneth Albanowski <kjahds@kjahds.com>
 *
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <linux/fb.h>


#define arraysize(x)    (sizeof(x)/sizeof(*(x)))

struct mc68328_fb_par {
   int xres;
   int yres;
   int bpp;
};

static struct mc68328_fb_par current_par;

static int current_par_valid = 0;
static int currcon = 0;

static struct display disp[MAX_NR_CONSOLES];
static struct fb_info fb_info;

static int node;        /* node of the /dev/fb?current file */


   /*
    *    Switch for Chipset Independency
    */

static struct fb_hwswitch {

   /* Initialisation */

   int (*init)(void);

   /* Display Control */

   int (*encode_fix)(struct fb_fix_screeninfo *fix, struct mc68328_fb_par *par);
   int (*decode_var)(struct fb_var_screeninfo *var, struct mc68328_fb_par *par);
   int (*encode_var)(struct fb_var_screeninfo *var, struct mc68328_fb_par *par);
   int (*getcolreg)(u_int regno, u_int *red, u_int *green, u_int *blue,
                    u_int *transp);
   int (*setcolreg)(u_int regno, u_int red, u_int green, u_int blue,
                    u_int transp);
   void (*blank)(int blank);
} *fbhw;


   /*
    *    Frame Buffer Name
    */

static char mc68328_fb_name[16] = "mc68328";


   /*
    *    mc68328vision Graphics Board
    */

#define CYBER8_WIDTH 1152
#define CYBER8_HEIGHT 886
#define CYBER8_PIXCLOCK 12500    /* ++Geert: Just a guess */

#define CYBER16_WIDTH 800
#define CYBER16_HEIGHT 600
#define CYBER16_PIXCLOCK 25000   /* ++Geert: Just a guess */

#define PALM_WIDTH 160
#define PALM_HEIGHT 160


/*static int mc68328Key = 0;
static u_char mc68328_colour_table [256][4];*/
static unsigned long mc68328Mem;
static unsigned long mc68328Size;

static long *memstart;


   /*
    *    Predefined Video Mode Names
    */

static char *mc68328_fb_modenames[] = {

   /*
    *    Autodetect (Default) Video Mode
    */

   "default",

   /*
    *    Predefined Video Modes
    */
    
   "Palm",            /* Palm Pilot devices, 1.0 and higher */
   "Palm Grey",            /* Palm Pilot devices, 1.0 and higher */

   /*
    *    Dummy Video Modes
    */

   "dummy", "dummy", "dummy", "dummy", "dummy", "dummy", "dummy", "dummy",
   "dummy", "dummy", "dummy", "dummy", "dummy", "dummy", "dummy", "dummy",
   "dummy", "dummy", "dummy", "dummy",

   /*
    *    User Defined Video Modes
    *
    *    This doesn't work yet!!
    */

   "user0", "user1", "user2", "user3", "user4", "user5", "user6", "user7"
};


   /*
    *    Predefined Video Mode Definitions
    */

static struct fb_var_screeninfo mc68328_fb_predefined[] = {

   /*
    *    Autodetect (Default) Video Mode
    */

   { 0, },

   /*
    *    Predefined Video Modes
    */
    
   {
      /* Palm */
      PALM_WIDTH, PALM_HEIGHT, PALM_WIDTH, PALM_HEIGHT, 
      0, 0, 
      1, -1,
      {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, 0},
      0, 0, 
      -1, -1, /* phys height, width */
      FB_ACCEL_NONE, 
      0, 0, 0, 0, 0, 0, 0, /* timing */
      0, /* sync */
      FB_VMODE_NONINTERLACED
   },
   {
      /* Palm Grey */
      PALM_WIDTH, PALM_HEIGHT, PALM_WIDTH, PALM_HEIGHT, 
      0, 0, 
      2, -1,
      {0, 2, 0}, {0, 2, 0}, {0, 2, 0}, {0, 0, 0},
      0, 0, 
      -1, -1, /* phys height, width */
      FB_ACCEL_NONE, 
      0, 0, 0, 0, 0, 0, 0, /* timing */
      0, /* sync */
      FB_VMODE_NONINTERLACED
   },

   /*
    *    Dummy Video Modes
    */

   { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, },
   { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, },
   { 0, }, { 0, },

   /*
    *    User Defined Video Modes
    */

   { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }, { 0, }
};


#define NUM_TOTAL_MODES    arraysize(mc68328_fb_predefined)
#define NUM_PREDEF_MODES   (3)


static int mc68328fb_inverse = 0;
static int mc68328fb_mode = 0;
static int mc68328fbCursorMode = 0;


   /*
    *    Some default modes
    */

#define PALM_DEFMODE     (1)
#define CYBER16_DEFMODE    (2)


   /*
    *    Interface used by the world
    */

void mc68328_video_setup(char *options, int *ints);

static int mc68328_fb_get_fix(struct fb_fix_screeninfo *fix, int con);
static int mc68328_fb_get_var(struct fb_var_screeninfo *var, int con);
static int mc68328_fb_set_var(struct fb_var_screeninfo *var, int con);
static int mc68328_fb_get_cmap(struct fb_cmap *cmap, int kspc, int con);
static int mc68328_fb_set_cmap(struct fb_cmap *cmap, int kspc, int con);
static int mc68328_fb_pan_display(struct fb_var_screeninfo *var, int con);
static int mc68328_fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
                          u_long arg, int con);


   /*
    *    Interface to the low level console driver
    */

struct fb_info *mc68328_fb_init(long *mem_start); /* Through amiga_fb_init() */
static int mc68328fb_switch(int con);
static int mc68328fb_updatevar(int con);
static void mc68328fb_blank(int blank);


   /*
    *    Accelerated Functions used by the low level console driver
    */

void mc68328_WaitQueue(u_short fifo);
void mc68328_WaitBlit(void);
void mc68328_BitBLT(u_short curx, u_short cury, u_short destx, u_short desty,
                  u_short width, u_short height, u_short mode);
void mc68328_RectFill(u_short x, u_short y, u_short width, u_short height,
                    u_short mode, u_short color);
void mc68328_MoveCursor(u_short x, u_short y);


   /*
    *   Hardware Specific Routines
    */

static int mc68328_init(void);
static int mc68328_encode_fix(struct fb_fix_screeninfo *fix,
                          struct mc68328_fb_par *par);
static int mc68328_decode_var(struct fb_var_screeninfo *var,
                          struct mc68328_fb_par *par);
static int mc68328_encode_var(struct fb_var_screeninfo *var,
                          struct mc68328_fb_par *par);
static int mc68328_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp);
static int mc68328_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp);
static void mc68328_blank(int blank);


   /*
    *    Internal routines
    */

static void mc68328_fb_get_par(struct mc68328_fb_par *par);
static void mc68328_fb_set_par(struct mc68328_fb_par *par);
static int do_fb_set_var(struct fb_var_screeninfo *var, int isactive);
static struct fb_cmap *get_default_colormap(int bpp);
static int do_fb_get_cmap(struct fb_cmap *cmap, struct fb_var_screeninfo *var,
                          int kspc);
static int do_fb_set_cmap(struct fb_cmap *cmap, struct fb_var_screeninfo *var,
                          int kspc);
static void do_install_cmap(int con);
static void memcpy_fs(int fsfromto, void *to, void *from, int len);
static void copy_cmap(struct fb_cmap *from, struct fb_cmap *to, int fsfromto);
static int alloc_cmap(struct fb_cmap *cmap, int len, int transp);
static void mc68328_fb_set_disp(int con);
static int get_video_mode(const char *name);


/* -------------------- Hardware specific routines -------------------------- */


   /*
    *    Initialization
    *
    *    Set the default video mode for this chipset. If a video mode was
    *    specified on the command line, it will override the default mode.
    */

static int mc68328_init(void)
{
	/*int i;
	char size;
	volatile u_long *CursorBase;
	unsigned long board_addr;
	struct ConfigDev *cd;*/

	if (mc68328fb_mode == -1)
		mc68328fb_mode = PALM_DEFMODE;

	mc68328Mem = (*(volatile unsigned long*)0xFFFFFA00);
	/*kernel_map (board_addr + 0x01400000, 0x00400000,*/
	mc68328Size = 160*160/8;

	return (0);
}


   /*
    *    This function should fill in the `fix' structure based on the
    *    values in the `par' structure.
    */

static int mc68328_encode_fix(struct fb_fix_screeninfo *fix,
                          struct mc68328_fb_par *par)
{
   int i;

   strcpy(fix->id, mc68328_fb_name);
   fix->smem_start = mc68328Mem;
   fix->smem_len = mc68328Size;

   fix->type = FB_TYPE_PACKED_PIXELS;
   fix->type_aux = 0;
   if (par->bpp == 1)
      fix->visual = FB_VISUAL_MONO01;
   else
      fix->visual = FB_VISUAL_DIRECTCOLOR;

   fix->xpanstep = 0;
   fix->ypanstep = 0;
   fix->ywrapstep = 0;

   for (i = 0; i < arraysize(fix->reserved); i++)
      fix->reserved[i] = 0;

   return(0);
}


   /*
    *    Get the video params out of `var'. If a value doesn't fit, round
    *    it up, if it's too big, return -EINVAL.
    */

static int mc68328_decode_var(struct fb_var_screeninfo *var,
                          struct mc68328_fb_par *par)
{
   par->xres = PALM_WIDTH;
   par->yres = PALM_HEIGHT;
   par->bpp = 1;
   return(0);
}


   /*
    *    Fill the `var' structure based on the values in `par' and maybe
    *    other values read out of the hardware.
    */

static int mc68328_encode_var(struct fb_var_screeninfo *var,
                          struct mc68328_fb_par *par)
{
   int i;

   var->xres = par->xres;
   var->yres = par->yres;
   var->xres_virtual = par->xres;
   var->yres_virtual = par->yres;
   var->xoffset = 0;
   var->yoffset = 0;

   var->bits_per_pixel = par->bpp;
   var->grayscale = -1;

   var->red.offset = 0;
   var->red.length = par->bpp;
   var->red.msb_right = 0;
   var->blue = var->green = var->red;

   var->transp.offset = 0;
   var->transp.length = 0;
   var->transp.msb_right = 0;

   var->nonstd = 0;
   var->activate = 0;

   var->height = -1;
   var->width = -1;
   var->accel = FB_ACCEL_NONE;
   var->vmode = FB_VMODE_NONINTERLACED;

   /* Dummy values */

   var->pixclock = 0;
   var->sync = 0;
   var->left_margin = 0;
   var->right_margin = 0;
   var->upper_margin = 0;
   var->lower_margin = 0;
   var->hsync_len = 0;
   var->vsync_len = 0;

   for (i = 0; i < arraysize(var->reserved); i++)
      var->reserved[i] = 0;

   return(0);
}


   /*
    *    Set a single color register. The values supplied are already
    *    rounded down to the hardware's capabilities (according to the
    *    entries in the var structure). Return != 0 for invalid regno.
    */

static int mc68328_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp)
{
	return 1;
#if 0
if (regno > 255)
  return (1);

*(mc68328Regs + 0x3c8) = (char)regno;
mc68328_colour_table [regno][0] = red & 0xff;
mc68328_colour_table [regno][1] = green & 0xff;
mc68328_colour_table [regno][2] = blue & 0xff;
mc68328_colour_table [regno][3] = transp;

*(mc68328Regs + 0x3c9) = (red & 0xff) >> 2;
*(mc68328Regs + 0x3c9) = (green & 0xff) >> 2;
*(mc68328Regs + 0x3c9) = (blue & 0xff) >> 2;

return (0);
#endif
}


   /*
    *    Read a single color register and split it into
    *    colors/transparent. Return != 0 for invalid regno.
    */

static int mc68328_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp)
{
	return 1;
#if 0
if (regno >= 256)
  return (1);
*red    = mc68328_colour_table [regno][0];
*green  = mc68328_colour_table [regno][1];
*blue   = mc68328_colour_table [regno][2];
*transp = mc68328_colour_table [regno][3];
return (0);
#endif
}


   /*
    *    (Un)Blank the screen
    */

void mc68328_blank(int blank)
{
#if 0
	if (blank)
		(*(volatile unsigned char*)0xFFFFFA27) &= ~128;
	else
		(*(volatile unsigned char*)0xFFFFFA27) |= 128;
#endif
}


/**************************************************************
 * We are waiting for "fifo" FIFO-slots empty
 */
void mc68328_WaitQueue (u_short fifo)
{
}

/**************************************************************
 * We are waiting for Hardware (Graphics Engine) not busy
 */
void mc68328_WaitBlit (void)
{
}

/**************************************************************
 * BitBLT - Through the Plane
 */
void mc68328_BitBLT (u_short curx, u_short cury, u_short destx, u_short desty,
                   u_short width, u_short height, u_short mode)
{
#if 0
u_short blitcmd = S3_BITBLT;

/* Set drawing direction */
/* -Y, X maj, -X (default) */
if (curx > destx)
  blitcmd |= 0x0020;  /* Drawing direction +X */
else
  {
  curx  += (width - 1);
  destx += (width - 1);
  }

if (cury > desty)
  blitcmd |= 0x0080;  /* Drawing direction +Y */
else
  {
  cury  += (height - 1);
  desty += (height - 1);
  }

mc68328_WaitQueue (0x8000);

*((u_short volatile *)(mc68328Regs + S3_PIXEL_CNTL)) = 0xa000;
*((u_short volatile *)(mc68328Regs + S3_FRGD_MIX)) = (0x0060 | mode);

*((u_short volatile *)(mc68328Regs + S3_CUR_X)) = curx;
*((u_short volatile *)(mc68328Regs + S3_CUR_Y)) = cury;

*((u_short volatile *)(mc68328Regs + S3_DESTX_DIASTP)) = destx;
*((u_short volatile *)(mc68328Regs + S3_DESTY_AXSTP)) = desty;

*((u_short volatile *)(mc68328Regs + S3_MIN_AXIS_PCNT)) = height - 1;
*((u_short volatile *)(mc68328Regs + S3_MAJ_AXIS_PCNT)) = width  - 1;

*((u_short volatile *)(mc68328Regs + S3_CMD)) = blitcmd;
#endif
}

/**************************************************************
 * Rectangle Fill Solid
 */
void mc68328_RectFill (u_short x, u_short y, u_short width, u_short height,
                     u_short mode, u_short color)
{
#if 0
u_short blitcmd = S3_FILLEDRECT;

mc68328_WaitQueue (0x8000);

*((u_short volatile *)(mc68328Regs + S3_PIXEL_CNTL)) = 0xa000;
*((u_short volatile *)(mc68328Regs + S3_FRGD_MIX)) = (0x0020 | mode);

*((u_short volatile *)(mc68328Regs + S3_MULT_MISC)) = 0xe000;
*((u_short volatile *)(mc68328Regs + S3_FRGD_COLOR)) = color;

*((u_short volatile *)(mc68328Regs + S3_CUR_X)) = x;
*((u_short volatile *)(mc68328Regs + S3_CUR_Y)) = y;

*((u_short volatile *)(mc68328Regs + S3_MIN_AXIS_PCNT)) = height - 1;
*((u_short volatile *)(mc68328Regs + S3_MAJ_AXIS_PCNT)) = width  - 1;

*((u_short volatile *)(mc68328Regs + S3_CMD)) = blitcmd;
#endif
}


/**************************************************************
 * Move cursor to x, y
 */
void mc68328_MoveCursor (u_short x, u_short y)
{
	(*(volatile unsigned short*)0xFFFFFA18) = (mc68328fbCursorMode << 14) | x;
	(*(volatile unsigned short*)0xFFFFFA1A) = y;
#if 0
*(mc68328Regs + S3_CRTC_ADR)  = 0x39;
*(mc68328Regs + S3_CRTC_DATA) = 0xa0;

*(mc68328Regs + S3_CRTC_ADR)  = S3_HWGC_ORGX_H;
*(mc68328Regs + S3_CRTC_DATA) = (char)((x & 0x0700) >> 8);
*(mc68328Regs + S3_CRTC_ADR)  = S3_HWGC_ORGX_L;
*(mc68328Regs + S3_CRTC_DATA) = (char)(x & 0x00ff);

*(mc68328Regs + S3_CRTC_ADR)  = S3_HWGC_ORGY_H;
*(mc68328Regs + S3_CRTC_DATA) = (char)((y & 0x0700) >> 8);
*(mc68328Regs + S3_CRTC_ADR)  = S3_HWGC_ORGY_L;
*(mc68328Regs + S3_CRTC_DATA) = (char)(y & 0x00ff);
#endif
}


/* -------------------- Interfaces to hardware functions -------------------- */


static struct fb_hwswitch mc68328_switch = {
   mc68328_init, mc68328_encode_fix, mc68328_decode_var, mc68328_encode_var,
   mc68328_getcolreg, mc68328_setcolreg, mc68328_blank
};


/* -------------------- Generic routines ------------------------------------ */


   /*
    *    Fill the hardware's `par' structure.
    */

static void mc68328_fb_get_par(struct mc68328_fb_par *par)
{
   if (current_par_valid)
      *par = current_par;
   else
      fbhw->decode_var(&mc68328_fb_predefined[mc68328fb_mode], par);
}


static void mc68328_fb_set_par(struct mc68328_fb_par *par)
{
   current_par = *par;
   current_par_valid = 1;
}


static int do_fb_set_var(struct fb_var_screeninfo *var, int isactive)
{
   int err, activate;
   struct mc68328_fb_par par;

   if ((err = fbhw->decode_var(var, &par)))
      return(err);
   activate = var->activate;
   if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW && isactive)
      mc68328_fb_set_par(&par);
   fbhw->encode_var(var, &par);
   var->activate = activate;
   return(0);
}


   /*
    *    Default Colormaps
    */

static u_short red16[] =
   { 0xc000, 0x0000, 0x0000, 0x0000, 0xc000, 0xc000, 0xc000, 0x0000,
     0x8000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff};
static u_short green16[] =
   { 0xc000, 0x0000, 0xc000, 0xc000, 0x0000, 0x0000, 0xc000, 0x0000,
     0x8000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff};
static u_short blue16[] =
   { 0xc000, 0x0000, 0x0000, 0xc000, 0x0000, 0xc000, 0x0000, 0x0000,
     0x8000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff};


static struct fb_cmap default_16_colors =
   { 0, 16, red16, green16, blue16, NULL };


static struct fb_cmap *get_default_colormap(int bpp)
{
   return(&default_16_colors);
}


#define CNVT_TOHW(val,width)     ((((val)<<(width))+0x7fff-(val))>>16)
#define CNVT_FROMHW(val,width)   (((width) ? ((((val)<<16)-(val)) / \
                                              ((1<<(width))-1)) : 0))

static int do_fb_get_cmap(struct fb_cmap *cmap, struct fb_var_screeninfo *var,
                          int kspc)
{
   int i, start;
   u_short *red, *green, *blue, *transp;
   u_int hred, hgreen, hblue, htransp;

   red = cmap->red;
   green = cmap->green;
   blue = cmap->blue;
   transp = cmap->transp;
   start = cmap->start;
   if (start < 0)
      return(-EINVAL);
   for (i = 0; i < cmap->len; i++) {
      if (fbhw->getcolreg(start++, &hred, &hgreen, &hblue, &htransp))
         return(0);
      hred = CNVT_FROMHW(hred, var->red.length);
      hgreen = CNVT_FROMHW(hgreen, var->green.length);
      hblue = CNVT_FROMHW(hblue, var->blue.length);
      htransp = CNVT_FROMHW(htransp, var->transp.length);
      if (kspc) {
         *red = hred;
         *green = hgreen;
         *blue = hblue;
         if (transp)
            *transp = htransp;
      } else {
         put_fs_word(hred, red);
         put_fs_word(hgreen, green);
         put_fs_word(hblue, blue);
         if (transp)
            put_fs_word(htransp, transp);
      }
      red++;
      green++;
      blue++;
      if (transp)
         transp++;
   }
   return(0);
}


static int do_fb_set_cmap(struct fb_cmap *cmap, struct fb_var_screeninfo *var,
                          int kspc)
{
   int i, start;
   u_short *red, *green, *blue, *transp;
   u_int hred, hgreen, hblue, htransp;

   red = cmap->red;
   green = cmap->green;
   blue = cmap->blue;
   transp = cmap->transp;
   start = cmap->start;

   if (start < 0)
      return(-EINVAL);
   for (i = 0; i < cmap->len; i++) {
      if (kspc) {
         hred = *red;
         hgreen = *green;
         hblue = *blue;
         htransp = transp ? *transp : 0;
      } else {
         hred = get_fs_word(red);
         hgreen = get_fs_word(green);
         hblue = get_fs_word(blue);
         htransp = transp ? get_fs_word(transp) : 0;
      }
      hred = CNVT_TOHW(hred, var->red.length);
      hgreen = CNVT_TOHW(hgreen, var->green.length);
      hblue = CNVT_TOHW(hblue, var->blue.length);
      htransp = CNVT_TOHW(htransp, var->transp.length);
      red++;
      green++;
      blue++;
      if (transp)
         transp++;
      if (fbhw->setcolreg(start++, hred, hgreen, hblue, htransp))
         return(0);
   }
   return(0);
}


static void do_install_cmap(int con)
{
   if (con != currcon)
      return;
   if (disp[con].cmap.len)
      do_fb_set_cmap(&disp[con].cmap, &disp[con].var, 1);
   else
      do_fb_set_cmap(get_default_colormap(disp[con].var.bits_per_pixel),
                                          &disp[con].var, 1);
}


static void memcpy_fs(int fsfromto, void *to, void *from, int len)
{
   switch (fsfromto) {
      case 0:
         memcpy(to, from, len);
         return;
      case 1:
         memcpy_fromfs(to, from, len);
         return;
      case 2:
         memcpy_tofs(to, from, len);
         return;
   }
}


static void copy_cmap(struct fb_cmap *from, struct fb_cmap *to, int fsfromto)
{
   int size;
   int tooff = 0, fromoff = 0;

   if (to->start > from->start)
      fromoff = to->start-from->start;
   else
      tooff = from->start-to->start;
   size = to->len-tooff;
   if (size > from->len-fromoff)
      size = from->len-fromoff;
   if (size < 0)
      return;
   size *= sizeof(u_short);
   memcpy_fs(fsfromto, to->red+tooff, from->red+fromoff, size);
   memcpy_fs(fsfromto, to->green+tooff, from->green+fromoff, size);
   memcpy_fs(fsfromto, to->blue+tooff, from->blue+fromoff, size);
   if (from->transp && to->transp)
      memcpy_fs(fsfromto, to->transp+tooff, from->transp+fromoff, size);
}


static int alloc_cmap(struct fb_cmap *cmap, int len, int transp)
{
   int size = len*sizeof(u_short);

   if (cmap->len != len) {
      if (cmap->red)
         kfree(cmap->red);
      if (cmap->green)
         kfree(cmap->green);
      if (cmap->blue)
         kfree(cmap->blue);
      if (cmap->transp)
         kfree(cmap->transp);
      cmap->red = cmap->green = cmap->blue = cmap->transp = NULL;
      cmap->len = 0;
      if (!len)
         return(0);
      if (!(cmap->red = kmalloc(size, GFP_ATOMIC)))
         return(-1);
      if (!(cmap->green = kmalloc(size, GFP_ATOMIC)))
         return(-1);
      if (!(cmap->blue = kmalloc(size, GFP_ATOMIC)))
         return(-1);
      if (transp) {
         if (!(cmap->transp = kmalloc(size, GFP_ATOMIC)))
            return(-1);
      } else
         cmap->transp = NULL;
   }
   cmap->start = 0;
   cmap->len = len;
   copy_cmap(get_default_colormap(len), cmap, 0);
   return(0);
}


   /*
    *    Get the Fixed Part of the Display
    */

static int mc68328_fb_get_fix(struct fb_fix_screeninfo *fix, int con)
{
   struct mc68328_fb_par par;
   int error = 0;

   if (con == -1)
      mc68328_fb_get_par(&par);
   else
      error = fbhw->decode_var(&disp[con].var, &par);
   return(error ? error : fbhw->encode_fix(fix, &par));
}


   /*
    *    Get the User Defined Part of the Display
    */

static int mc68328_fb_get_var(struct fb_var_screeninfo *var, int con)
{
   struct mc68328_fb_par par;
   int error = 0;

   if (con == -1) {
      mc68328_fb_get_par(&par);
      error = fbhw->encode_var(var, &par);
   } else
      *var = disp[con].var;
   return(error);
}


static void mc68328_fb_set_disp(int con)
{
   struct fb_fix_screeninfo fix;

   mc68328_fb_get_fix(&fix, con);
   if (con == -1)
      con = 0;
   disp[con].screen_base = (u_char *)fix.smem_start;
   disp[con].visual = fix.visual;
   disp[con].type = fix.type;
   disp[con].type_aux = fix.type_aux;
   disp[con].ypanstep = fix.ypanstep;
   disp[con].ywrapstep = fix.ywrapstep;
   disp[con].can_soft_blank = 1;
   disp[con].inverse = mc68328fb_inverse;
}


   /*
    *    Set the User Defined Part of the Display
    */

static int mc68328_fb_set_var(struct fb_var_screeninfo *var, int con)
{
   int err, oldxres, oldyres, oldvxres, oldvyres, oldbpp;

   if ((err = do_fb_set_var(var, con == currcon)))
      return(err);
   if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
      oldxres = disp[con].var.xres;
      oldyres = disp[con].var.yres;
      oldvxres = disp[con].var.xres_virtual;
      oldvyres = disp[con].var.yres_virtual;
      oldbpp = disp[con].var.bits_per_pixel;
      disp[con].var = *var;
      if (oldxres != var->xres || oldyres != var->yres ||
          oldvxres != var->xres_virtual || oldvyres != var->yres_virtual ||
          oldbpp != var->bits_per_pixel) {
         mc68328_fb_set_disp(con);
         (*fb_info.changevar)(con);
         alloc_cmap(&disp[con].cmap, 0, 0);
         do_install_cmap(con);
      }
   }
   var->activate = 0;
   return(0);
}


   /*
    *    Get the Colormap
    */

static int mc68328_fb_get_cmap(struct fb_cmap *cmap, int kspc, int con)
{
   if (con == currcon) /* current console? */
      return(do_fb_get_cmap(cmap, &disp[con].var, kspc));
   else if (disp[con].cmap.len) /* non default colormap? */
      copy_cmap(&disp[con].cmap, cmap, kspc ? 0 : 2);
   else
      copy_cmap(get_default_colormap(disp[con].var.bits_per_pixel), cmap,
                kspc ? 0 : 2);
   return(0);
}


   /*
    *    Set the Colormap
    */

static int mc68328_fb_set_cmap(struct fb_cmap *cmap, int kspc, int con)
{
   int err;

   if (!disp[con].cmap.len) {       /* no colormap allocated? */
      if ((err = alloc_cmap(&disp[con].cmap, 1<<disp[con].var.bits_per_pixel,
                            0)))
         return(err);
   }
   if (con == currcon)              /* current console? */
      return(do_fb_set_cmap(cmap, &disp[con].var, kspc));
   else
      copy_cmap(cmap, &disp[con].cmap, kspc ? 0 : 1);
   return(0);
}


   /*
    *    Pan or Wrap the Display
    *
    *    This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
    */

static int mc68328_fb_pan_display(struct fb_var_screeninfo *var, int con)
{
   return(-EINVAL);
}


   /*
    *    mc68328 Frame Buffer Specific ioctls
    */

static int mc68328_fb_ioctl(struct inode *inode, struct file *file,
                          u_int cmd, u_long arg, int con)
{
   return(-EINVAL);
}


static struct fb_ops mc68328_fb_ops = {
   mc68328_fb_get_fix, mc68328_fb_get_var, mc68328_fb_set_var, mc68328_fb_get_cmap,
   mc68328_fb_set_cmap, mc68328_fb_pan_display, mc68328_fb_ioctl
};


void mc68328_video_setup(char *options, int *ints)
{
   char *this_opt;
   int i;

   fb_info.fontname[0] = '\0';

   if (!options || !*options)
      return;

   for (this_opt = strtok(options, ","); this_opt; this_opt = strtok(NULL, ","))
      if (!strcmp(this_opt, "inverse")) {
         mc68328fb_inverse = 1;
         for (i = 0; i < 16; i++) {
            red16[i] = ~red16[i];
            green16[i] = ~green16[i];
            blue16[i] = ~blue16[i];
         }
      } else if (!strncmp(this_opt, "font:", 5))
         strcpy(fb_info.fontname, this_opt+5);
      else
         mc68328fb_mode = get_video_mode(this_opt);
}


   /*
    *    Initialization
    */

struct fb_info *mc68328_fb_init(long *mem_start)
{
   int err;
   struct mc68328_fb_par par;

   memstart = mem_start;

   fbhw = &mc68328_switch;

   err = register_framebuffer(mc68328_fb_name, &node, &mc68328_fb_ops,
                              NUM_TOTAL_MODES, mc68328_fb_predefined);
   if (err < 0)
      panic("Cannot register frame buffer\n");

   fbhw->init();
   fbhw->decode_var(&mc68328_fb_predefined[mc68328fb_mode], &par);
   fbhw->encode_var(&mc68328_fb_predefined[0], &par);

   strcpy(fb_info.modename, mc68328_fb_name);
   fb_info.disp = disp;
   fb_info.switch_con = &mc68328fb_switch;
   fb_info.updatevar = &mc68328fb_updatevar;
   fb_info.blank = &mc68328fb_blank;

   do_fb_set_var(&mc68328_fb_predefined[0], 1);
   mc68328_fb_get_var(&disp[0].var, -1);
   mc68328_fb_set_disp(-1);
   do_install_cmap(0);
   return(&fb_info);
}


static int mc68328fb_switch(int con)
{
   /* Do we have to save the colormap? */
   if (disp[currcon].cmap.len)
      do_fb_get_cmap(&disp[currcon].cmap, &disp[currcon].var, 1);

   do_fb_set_var(&disp[con].var, 1);
   currcon = con;
   /* Install new colormap */
   do_install_cmap(con);
   return(0);
}


   /*
    *    Update the `var' structure (called by fbcon.c)
    *
    *    This call looks only at yoffset and the FB_VMODE_YWRAP flag in `var'.
    *    Since it's called by a kernel driver, no range checking is done.
    */

static int mc68328fb_updatevar(int con)
{
   return(0);
}


   /*
    *    Blank the display.
    */

static void mc68328fb_blank(int blank)
{
   fbhw->blank(blank);
}


   /*
    *    Get a Video Mode
    */

static int get_video_mode(const char *name)
{
   int i;

   for (i = 1; i < NUM_PREDEF_MODES; i++)
      if (!strcmp(name, mc68328_fb_modenames[i]))
         return(i);
   return(0);
}
