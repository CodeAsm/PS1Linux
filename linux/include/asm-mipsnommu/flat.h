
/* Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>
 *                     The Silver Hammer Group, Ltd.
 *
 */

#ifndef _LINUX_FLAT_H
#define _LINUX_FLAT_H

struct flat_hdr {
	char magic[4];
	unsigned long rev;
   unsigned long entry_point; /* Offset of program start point from beginning of text segment */
	unsigned long text_start; /* Offset of first executable instruction with text segment from beginning of file*/
	unsigned long data_start; /* Offset of data segment from beginning of file*/
	
	unsigned long data_end; /* Offset of end of data segment from beginning of file*/
	unsigned long bss_end; /* Offset of end of bss segment from beginning of file*/
				/* (It is assumed that data_end through bss_end forms the
				    bss segment.) */
	unsigned long stack_size; /* Size of stack, in bytes */
	unsigned long reloc_start; /* Offset of relocation records from beginning of file */
	
	unsigned long reloc_count; /* Number of relocation records */
	
	unsigned long flags;     
	
	unsigned long filler[5]; /* Reservered, set to zero */
};

#define FLAT_RELOC_IN_TEXT       0x00000000
#define FLAT_RELOC_IN_DATA       0x40000000
#define FLAT_RELOC_IN_BSS        0x80000000
#define FLAT_RELOC_IN_MASK       0xc0000000
#define FLAT_RELOC_IN_SHIFT      (30)

#define FLAT_RELOC_REL_TEXT      0x00000000
#define FLAT_RELOC_REL_DATA      0x10000000
#define FLAT_RELOC_REL_BSS       0x20000000
#define FLAT_RELOC_REL_MASK      0x30000000
#define FLAT_RELOC_REL_SHIFT     (28)

#define FLAT_RELOC_TYPE_32       0x0000000
#define FLAT_RELOC_TYPE_HI16     0x1000000
#define FLAT_RELOC_TYPE_LO16     0x2000000
#define FLAT_RELOC_TYPE_26       0x3000000
#define FLAT_RELOC_TYPE_MASK     0xf000000
#define FLAT_RELOC_TYPE_SHIFT    (24)

#define FLAT_RELOC_SIGN_POS      0x000000
#define FLAT_RELOC_SIGN_NEG      0x800000
#define FLAT_RELOC_SIGN_MASK     0x800000
#define FLAT_RELOC_SIGN_SHIFT    (23)

#define FLAT_RELOC_OFFSET_MASK   0x7fffff

typedef unsigned long flat_reloc;   /* |in|rel|type|sign|offset| */
                                      /* 31 29  27   23   22     0 */

/*
 *	Endien issues - blak!
 */
#define FLAT_FLAG_RAM  0x01000000   /* program should be loaded entirely into RAM */

#endif /* _LINUX_FLAT_H */
