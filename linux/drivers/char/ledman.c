/****************************************************************************/
/*	vi:set tabstop=4 cindent shiftwidth=4:
 *
 *	ledman.c -- An LED manager,  primarily,  but not limited to Lineo devices
 *              manages up to 32 seperate LED at once.
 *	            Copyright (C) Lineo, 2000.
 *
 *	This driver currently supports 4 types of LED modes:
 *
 *	SET      - transient LED's that show activity,  cleared at next poll
 *	ON       - always ON
 *	OFF      - always OFF
 *  FLASHING - a blinking LED with the frequency determinbe by the poll func
 *
 *	Hopefully for most cases, adding new HW with new LED patterns will be
 *	as simple as adding two tables, a small function and an entry in
 *	led_modes.  The tables being the map and the defaults while the
 *	function is the XXX_set function.
 *
 *	You can, however, add your own functions for XXX_bits, XXX_tick and
 *	take full control over all aspects of the LED's.
 */
/****************************************************************************/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/utsname.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/malloc.h>
#include <linux/timer.h>
#include <linux/ledman.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

/****************************************************************************/

static void ledman_poll(unsigned long arg);
static int  ledman_ioctl(struct inode * inode, struct file * file,
								unsigned int cmd, unsigned long arg);
static int	ledman_bits(unsigned long cmd, unsigned long bits);
static void	ledman_tick(void);

/****************************************************************************/

static struct timer_list	ledman_timerlist;

/****************************************************************************/

struct file_operations ledman_fops = {
	ioctl: ledman_ioctl,	/* ledman_ioctl */
};

/****************************************************************************/
/*
 *	some types to make adding new LED modes easier
 *
 *	First the elements for def array specifying default LED behaviour
 */

#define LEDS_SET	0
#define LEDS_ON		1
#define LEDS_OFF	2
#define LEDS_FLASH	3
#define LEDS_MAX	4

typedef unsigned int leddef_t[LEDS_MAX];

/*
 *	a LED map is a mapping from numbers in ledman.h to one or more
 *	physical LED bits.  Currently the typing limits us to 32 LED's
 *	though this would not be hard to change
 */

typedef unsigned int ledmap_t[LEDMAN_MAX];

/*
 *	A LED mode is a definition of how a set of LED's should behave.
 *
 *	name    - a symbolic name for the LED mode,  used for changing modes
 *	map     - points to a ledmap array,  maps ledman.h defines to real LED bits
 *	def     - default behaviour for the LED bits (ie, on, flashing ...)
 *	bits    - perform command on physical bits,  you may use the default or
 *	          supply your own for more control.
 *	tick    - time based update of LED status,  used to clear SET LED's and
 *	          also for flashing LED's
 *	set     - set the real LED's to match the physical bits
 *	jiffies - how many clock ticks between runs of the tick routine.
 */

#define LEDMAN_MAX_NAME	16

typedef struct {
	char	name[LEDMAN_MAX_NAME];
	u_int	*map;
	u_int	*def;
	int		(*bits)(unsigned long cmd, unsigned long led);
	void	(*tick)(void);
	void	(*set)(unsigned long led);
	int		jiffies;
} ledmode_t;

/****************************************************************************/

static int current_mode = 0;				/* the default LED mode */
static int initted = 0;
static unsigned long leds_set, leds_on, leds_off, leds_flash;

/****************************************************************************/
/*
 *	Let the system specific defining begin
 */

#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
extern ledmap_t	nettel_std;
extern ledmap_t	nettel_alt;
extern leddef_t	nettel_def;
static void nettel_set(unsigned long bits);
#endif

#if defined(CONFIG_NETtel) && defined(CONFIG_M5206e)
extern ledmap_t	nt1500_std;
extern leddef_t	nt1500_def;
static void nt1500_set(unsigned long bits);
#endif

#ifdef CONFIG_eLIA
extern ledmap_t	elia_std;
extern leddef_t	elia_def;
static void elia_set(unsigned long bits);
#endif

/****************************************************************************/

ledmode_t led_mode[] = {

#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
	{ "std", nettel_std, nettel_def, ledman_bits, ledman_tick, nettel_set, 1 },
	{ "alt", nettel_alt, nettel_def, ledman_bits, ledman_tick, nettel_set, 1 },
#endif

#if defined(CONFIG_NETtel) && defined(CONFIG_M5206e)
	{ "std", nt1500_std, nt1500_def, ledman_bits, ledman_tick, nt1500_set, 1 },
#endif

#ifdef CONFIG_eLIA
	{ "std", elia_std, elia_def, ledman_bits, ledman_tick, elia_set, 1 },
#endif

	{ "",  NULL, NULL, 0 }
};

/****************************************************************************/
/*
 *	boot arg processing ledman=mode
 */

void
ledman_setup(char *arg, int *ints)
{
	ledman_cmd(LEDMAN_CMD_MODE, (unsigned long) arg);
}

/****************************************************************************/

static int __init
ledman_init(void)
{
	printk(KERN_INFO "ledman: Copyright (C) Lineo, 2000.\n");

	if (register_chrdev(LEDMAN_MAJOR, "nled",  &ledman_fops) < 0) {
		printk("%s(%d): ledman_init() can't get Major %d\n",
				__FILE__, __LINE__, LEDMAN_MAJOR);
		return(-EBUSY);
	} 
/*
 *	set the LEDs up correctly at boot
 */
	ledman_cmd(LEDMAN_CMD_RESET, LEDMAN_ALL);
/*
 *	start the timer
 */
	if (led_mode[current_mode].tick)
		ledman_timerlist.expires = jiffies + led_mode[current_mode].jiffies;
	else
		ledman_timerlist.expires = jiffies + HZ;
	ledman_timerlist.function = ledman_poll;
	ledman_timerlist.data = 0;
	add_timer(&ledman_timerlist);

	initted = 1;
	return(0);
}

module_init(ledman_init);

/****************************************************************************/

static void
ledman_poll(unsigned long arg)
{
	if (led_mode[current_mode].tick) {
		(*led_mode[current_mode].tick)();
		ledman_timerlist.expires = jiffies + led_mode[current_mode].jiffies;
	} else
		ledman_timerlist.expires = jiffies + HZ;
	add_timer(&ledman_timerlist);
}

/****************************************************************************/

static int
ledman_ioctl(
	struct inode * inode,
	struct file * file,
	unsigned int cmd,
	unsigned long arg)
{
	char	mode[LEDMAN_MAX_NAME];
	int		i;

	if (cmd == LEDMAN_CMD_MODE) {
		for (i = 0; i < sizeof(mode) - 1; i++) {
			get_user(mode[i], (char *) (arg + i));
			if (!mode[i])
				break;
		}
		mode[i] = '\0';
		arg = (unsigned long) &mode[0];
	}
	return(ledman_cmd(cmd, arg));
}

/****************************************************************************/
/*
 *	cmd - from ledman.h
 *	led - led code from ledman.h
 *
 *	check parameters and then call
 */

int
ledman_cmd(int cmd, unsigned long led)
{
	ledmode_t	*lmp;
	int			i;

	switch (cmd) {
	case LEDMAN_CMD_SET:
	case LEDMAN_CMD_ON:
	case LEDMAN_CMD_OFF:
	case LEDMAN_CMD_FLASH:
	case LEDMAN_CMD_RESET:
		break;
	case LEDMAN_CMD_MODE:
		for (i = 0; led_mode[i].name[0]; i++)
			if (strcmp((char *) led, led_mode[i].name) == 0) {
				current_mode = i;
				if (initted)
					ledman_cmd(LEDMAN_CMD_RESET, LEDMAN_ALL);
				return(0);
			}
		return(-EINVAL);
	default:
		return(-EINVAL);
	}

	if (led < 0 || led >= LEDMAN_MAX)
		return(-EINVAL);

	lmp = &led_mode[current_mode];
	(*lmp->bits)(cmd, lmp->map[led]);
	return(0);
}

/****************************************************************************/

static int
ledman_bits(unsigned long cmd, unsigned long bits)
{
	ledmode_t	*lmp = &led_mode[current_mode];

	switch (cmd) {
	case LEDMAN_CMD_SET:
		leds_set   |= bits;
		break;
	case LEDMAN_CMD_ON:
		leds_on    |= bits;
		leds_off   &= ~bits;
		leds_flash &= ~bits;
		(*lmp->tick)();
		break;
	case LEDMAN_CMD_OFF:
		leds_on    &= ~bits;
		leds_off   |= bits;
		leds_flash &= ~bits;
		(*lmp->tick)();
		break;
	case LEDMAN_CMD_FLASH:
		leds_on    &= ~bits;
		leds_off   &= ~bits;
		leds_flash |= bits;
		break;
	case LEDMAN_CMD_RESET:
		leds_set   = (leds_set   & ~bits) | (bits & lmp->def[LEDS_SET]);
		leds_on    = (leds_on    & ~bits) | (bits & lmp->def[LEDS_ON]);
		leds_off   = (leds_off   & ~bits) | (bits & lmp->def[LEDS_OFF]);
		leds_flash = (leds_flash & ~bits) | (bits & lmp->def[LEDS_FLASH]);
		break;
	default:
		return(-EINVAL);
	}
	return(0);
}

/****************************************************************************/

static void
ledman_tick(void)
{
	ledmode_t	*lmp = &led_mode[current_mode];
	int			new_value;
	static int	flash_on = 0;
/*
 *	work out which LED's should be on
 */
	new_value = ((leds_set | leds_on) & ~leds_off);
/*
 *	flashing LED's run on their own devices,  ie,  according to the
 *	value fo flash_on
 */
	if ((flash_on++ % 60) >= 30)
		new_value |= leds_flash;
	else
		new_value &= ~leds_flash;
/*
 *	set the HW
 */
 	(*lmp->set)(new_value);
	leds_set = 0;
}

/****************************************************************************/
#if defined(CONFIG_NETtel) && defined(CONFIG_M5307)
/****************************************************************************/
/*
 *	Here it the definition of the LED's on the NETtel circuit board
 *	as per the labels next to them.  The two parallel port LED's steal
 *	some high bits so we can map it more easily onto the HW
 *
 *	LED - D1   D2   D3   D4   D5   D6   D7   D8   D11  D12  
 *	HEX - 100  200  004  008  010  020  040  080  002  001
 *
 */

#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/nettel.h>

static ledmap_t	nettel_std = {
	0x3ff, 0x200, 0x100, 0x008, 0x004, 0x020, 0x010, 0x080, 0x080, 0x080,
	0x080, 0x040, 0x040, 0x002, 0x002, 0x024, 0x018, 0x001, 0x0ff, 0x0ff,
	0x000, 0x000,
};

static ledmap_t nettel_alt = {
	0x3ff, 0x200, 0x100, 0x040, 0x040, 0x002, 0x002, 0x008, 0x004, 0x020,
	0x010, 0x000, 0x000, 0x000, 0x000, 0x024, 0x018, 0x001, 0x0ff, 0x080,
	0x000, 0x000,
};

static leddef_t	nettel_def = {
	0x000, 0x200, 0x000, 0x100,
};


static void
nettel_set(unsigned long bits)
{
	int			pp;

	* (volatile char *) NETtel_LEDADDR = (~bits & 0xff);
	pp = ~(bits >> 3) & 0x60;
	ppdata = (ppdata & ~0x60) | pp;
	* ((volatile unsigned short *) (MCF_MBAR + MCFSIM_PADAT)) = ppdata;
}

/****************************************************************************/
#endif /* defined(CONFIG_NETtel) && defined(CONFIG_M5307) */
/****************************************************************************/
#if defined(CONFIG_NETtel) && defined(CONFIG_M5206e)
/****************************************************************************/
/*
 *	For the WebWhale/NETtel1500,  3 LED's (was 2)
 *
 *	LED - HEARTBEAT  DCD    DATA
 *  	HEX -    001     002    004
 */

#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/nettel.h>

static ledmap_t nt1500_std = {
	0x007, 0x000, 0x001, 0x004, 0x004, 0x004, 0x004, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x004, 0x002, 0x000, 0x007, 0x000,
	0x002, 0x002,
};

static leddef_t	nt1500_def = {
	0x000, 0x000, 0x000, 0x001,
};

static void
nt1500_set(unsigned long bits)
{
	* (volatile char *) NETtel_LEDADDR = (~bits & 0x7);
}

/****************************************************************************/
#endif /* defined(CONFIG_NETtel) && defined(CONFIG_M5206e) */
/****************************************************************************/
#ifdef CONFIG_eLIA
/****************************************************************************/
/*
 *	For the WebWhale,  only 2 LED's
 *
 *	LED - HEARTBEAT  USER
 *  	HEX -    2        1
 */

#ifdef CONFIG_COLDFIRE
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#endif
#include <asm/elia.h>

static ledmap_t elia_std = {
	0x003, 0x000, 0x002, 0x001, 0x001, 0x001, 0x001, 0x000, 0x000, 0x000,
	0x000, 0x000, 0x000, 0x000, 0x000, 0x002, 0x001, 0x000, 0x000, 0x000,
	0x000, 0x000,
};

static leddef_t	elia_def = {
	0x000, 0x000, 0x000, 0x002,
};

static void
elia_set(unsigned long bits)
{
	int			pp;

	pp = ~(bits << 12) & 0x3000;
	ppdata = (ppdata & ~0x3000) | pp;
	* ((volatile unsigned short *) (MCF_MBAR + MCFSIM_PADAT)) = ppdata;
}

/****************************************************************************/
#endif /* CONFIG_eLIA */
/****************************************************************************/
