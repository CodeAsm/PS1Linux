
/* 
 * linux/blkmem.h header file for Linux.
 *
 * Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>,
 *
 */

#ifndef _LINUX_BLKMEM_H
#define _LINUX_BLKMEM_H

#include <linux/ioctl.h>

/*
 * Structures and definitions for mag tape io control commands
 */
 
#define BMSERASE       1	/* erase sector containing address */
#define BMSGSIZE       2	/* get size of sector */
#define BMSGERASEVALUE 3	/* get value of bytes in erased sectors */
#define BMGETSIZES     4	/* get length of device in sectors */
#define BMGETSIZEB     5	/* get length of device in bytes */

#define BMPROGRAM      6	/* program entire arena in one go */

struct blkmem_program_t {
	unsigned long magic1;
	int	blocks;
	int	reset;
	unsigned long magic2;
	struct {
		unsigned char * data;
		unsigned long pos;
		unsigned long length;
		int magic3;
	}block[0];
};

#define BMPROGRAM_MAGIC_1 0x123abc32

#define BMPROGRAM_MAGIC_2 0x9C00C00F

#define BMPROGRAM_MAGIC_3 0x56408F26

#if 0
#define BMSSAUTOERASE  10	/* set auto-erase bits */
#endif

#define DEVICE_NAME "Blkmem"
#define DEVICE_REQUEST do_blkmem_request
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)

#endif /* _LINUX_BLKMEM_H */
