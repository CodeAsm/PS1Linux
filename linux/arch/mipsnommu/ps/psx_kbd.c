#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/pc_keyb.h>
#include <asm/keyboard.h>
#include <asm/io.h>
#include <asm/ps/interrupts.h>




//-----------KBD ???PSX

struct kbd_ops *kbd_ops;


static void psx_kbd_request_region(void)
{
}

static int psx_kbd_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
}

static unsigned char psx_kbd_read_input(void)
{
}

static void psx_kbd_write_output(unsigned char val)
{
}

static void psx_kbd_write_command(unsigned char val)
{
}

static unsigned char psx_kbd_read_status(void)
{
}


static int psx_aux_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
 return 0;
}

static void psx_aux_free_irq(void)
{
}


struct kbd_ops psxsiokbd_ops = {
	psx_kbd_request_region,
	psx_kbd_request_irq,
	psx_aux_request_irq,
	psx_aux_free_irq,

	psx_kbd_read_input,
	psx_kbd_write_output,
	psx_kbd_write_command,
	psx_kbd_read_status
};

psxkbd_hw_init()
{
}

int kbd_setkeycode(unsigned int scancode,unsigned int keycode){return 0;};
int kbd_getkeycode(unsigned int scancode){return 0;};
void kbd_leds(unsigned char leds){};
int kbd_translate(unsigned char scancode,unsigned char *keycode ,char raw_mode){return 0;};
char kbd_unexpected_up(unsigned char keycode){return 0;};
void kbd_init_hw(){};