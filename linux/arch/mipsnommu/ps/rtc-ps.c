#include <linux/mc146818rtc.h>
#include <asm/io.h>
#include <asm/ps/hwregs.h>
#include <asm/ps/pio_extension.h>

static int base = 0, size = 0;

static void rtc_access_on (void) {
	psx_page_switch_ret (REGS, base, size);
	outl (0x132677, PIO_BUS_PORT);
}	

static void rtc_access_off (void) {
	psx_page_switch_to (base, size);
}	

static unsigned char ps_rtc_read_data(unsigned long addr)
{
	unsigned char res;
	
	 rtc_access_on ();
    res = inb (PSX_RTC_BASE+addr);
    rtc_access_off ();
    return res;
}

static void ps_rtc_write_data(unsigned char data, unsigned long addr)
{
	 rtc_access_on ();
    outb (data, PSX_RTC_BASE+addr);
    rtc_access_off ();
}

static int ps_rtc_bcd_mode(void)
{
    return 0;
}

struct rtc_ops ps_rtc_ops =
{
    &ps_rtc_read_data,
    &ps_rtc_write_data,
    &ps_rtc_bcd_mode
};
