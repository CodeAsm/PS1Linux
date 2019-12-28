#ifndef __LINUX_LEDMAN_H__
#define __LINUX_LEDMAN_H__ 1
/****************************************************************************/
/*
 *	ledman.h:  LED manager header,  generic, device indepedant LED stuff
 *
 *	defines for led functionality which may/may not be implemented by the
 *	currently active LED configuration
 *
 *	NOTE: do not change the numbering of the defines below,  tables of
 *	      LED patterns rely on these values
 */

#define LEDMAN_ALL			0	/* special case, all LED's */

#define LEDMAN_POWER		1
#define LEDMAN_HEARTBEAT	2
#define LEDMAN_COM1_RX		3
#define LEDMAN_COM1_TX		4
#define LEDMAN_COM2_RX		5
#define LEDMAN_COM2_TX		6
#define LEDMAN_LAN1_RX		7
#define LEDMAN_LAN1_TX		8
#define LEDMAN_LAN2_RX		9
#define LEDMAN_LAN2_TX		10
#define LEDMAN_USB1_RX		11
#define LEDMAN_USB1_TX		12
#define LEDMAN_USB2_RX		13
#define LEDMAN_USB2_TX		14
#define LEDMAN_NVRAM_1		15
#define LEDMAN_NVRAM_2		16
#define LEDMAN_VPN			17
#define LEDMAN_LAN1_DHCP	18
#define LEDMAN_LAN2_DHCP	19
#define LEDMAN_COM1_DCD		20
#define LEDMAN_COM2_DCD		21

#define	LEDMAN_MAX			22	/* one more than the highest LED above */

/****************************************************************************/
/*
 *	ioctl cmds
 */

#define LEDMAN_CMD_SET		0x01	/* turn on briefly to show activity */
#define LEDMAN_CMD_ON		0x02	/* turn LED on permanently */
#define LEDMAN_CMD_OFF		0x03	/* turn LED off permanently */
#define LEDMAN_CMD_FLASH	0x04	/* flash this LED */
#define LEDMAN_CMD_RESET	0x05	/* reset LED to default behaviour */

#define LEDMAN_CMD_MODE		0x80	/* set LED to named mode (led=char *) */

/****************************************************************************/

#define LEDMAN_MAJOR	126

/****************************************************************************/
#ifdef __KERNEL__

extern void ledman_setup(char *arg, int *ints);
extern int  ledman_cmd(int cmd, unsigned long led);

#else

#include	<fcntl.h>

#define ledman_cmd(cmd, led) ({ \
	int fd; \
	if ((fd = open("/dev/ledman", O_RDWR)) != -1) { \
		ioctl(fd, cmd, led); \
		close(fd); \
	} \
})

#endif
/****************************************************************************/
#endif /* __LINUX_LEDMAN_H__ */
