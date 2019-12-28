
/* 
 * RTC routines.
 */
#include <asm/spinlock.h>
#include <linux/mc146818rtc.h>

extern char *rtc_base;

static unsigned char rtc_read_data(unsigned long addr)
{
    return (rtc_base[addr * 4]);
}

static void rtc_write_data(unsigned char data, unsigned long addr)
{
    rtc_base[addr * 4] = data;
}

static int rtc_bcd_mode(void)
{
    return 0;
}

struct rtc_ops ps_rtc_ops =
{
    &rtc_read_data,
    &rtc_write_data,
    &rtc_bcd_mode
};
