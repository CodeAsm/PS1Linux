#ifndef	__DRIVER_CHAR_SCAN_KEYB_H
#define	__DRIVER_CHAR_SCAN_KEYB_H
/*
 *	$Id: scan_keyb.h,v 1.1.1.1 2001/02/22 14:58:18 serg Exp $
 *	Copyright (C) 2000 YAEGASHI Takeshi
 *	Generic scan keyboard driver
 */

int register_scan_keyboard(void (*scan)(unsigned char *buffer),
			   const unsigned char *table,
			   int length);

void __init scan_kbd_init(void);

#endif
