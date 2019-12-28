
/* Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>
 *                     The Silver Hammer Group, Ltd.
 *
 */

#ifndef _LINUX_FLAT_H
#define _LINUX_FLAT_H

#include <asm/byteorder.h>

struct flat_hdr {
	char magic[4];
	unsigned long rev;
	unsigned long entry; /* Offset of first executable instruction with text segment from beginning of file*/
	unsigned long data_start; /* Offset of data segment from beginning of file*/
	
	unsigned long data_end; /* Offset of end of data segment from beginning of file*/
	unsigned long bss_end; /* Offset of end of bss segment from beginning of file*/
				/* (It is assumed that data_end through bss_end forms the
				    bss segment.) */
	unsigned long stack_size; /* Size of stack, in bytes */
	unsigned long reloc_start; /* Offset of relocation records from beginning of file */
	
	unsigned long reloc_count; /* Number of relocation records */
	
	unsigned long flags;       
	
	unsigned long filler[6]; /* Reservered, set to zero */
};

#define FLAT_RELOC_TYPE_TEXT 0
#define FLAT_RELOC_TYPE_DATA 1
#define FLAT_RELOC_TYPE_BSS 2

struct flat_reloc {
#if defined(__BIG_ENDIAN_BITFIELD) /* bit fields, ugh ! */
	unsigned long type : 2; 
	signed long offset : 30;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	signed long offset : 30;
	unsigned long type : 2; 
#else
#error "Unknown bitfield order for flat files."
#endif
};

#define FLAT_FLAG_RAM  0x0001    /* program should be loaded entirely into RAM */

#endif /* _LINUX_FLAT_H */
