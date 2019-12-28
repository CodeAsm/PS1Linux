/*
 * PlayStation PIO extension ports
 */

#ifndef __ASM_PS_PIO_EXTENSION_H 
#define __ASM_PS_PIO_EXTENSION_H 

#include <asm/io.h>
#include <asm/ps/hwregs.h>
#include <asm/system.h>

/* Top PIO memory (1f000000-1f03ffff) layout register ports */
#define PSX_TOP_TO_FLASH	0x100000
#define PSX_TOP_TO_RAM	  0x100001

/* Bottom PIO memory (1f980000-1f9bffff) layout register ports */
#define PSX_BOTTOM_TO_FLASH	0x100002
#define PSX_BOTTOM_TO_RAM	  0x100003

/* SDRAM initialization command ports */
#define PSX_PRECHARGE		0x108005
#define PSX_AUTOREFRESH	 0x108006
#define PSX_MODE_WRITE	  0x108007

/* USB data ports */
#define PSX_USB_0	0x140002
#define PSX_USB_1	0x140003

/* SmartMedia register ports */
#define PSX_SMARTMEDIA_DATA		0x140010
#define PSX_SMARTMEDIA_CONTROL	0x140011
#define PSX_SMARTMEDIA_STATUS	 0x140012

/* Interrupt register ports */
#define PSX_INTERRUPT_REQUEST	0x140014
#define PSX_INTERRUPT_MASK		0x140015

/* RTC data base address */
#define PSX_RTC_BASE	0x140080

/* Extension bus base address */
#define PSX_EXTENSION_BASE	0x140100

/* PIO pages parameters */
#define PSX_PAGE_A_BASE	     0x1f000000
#define PSX_PAGE_A_SIZE	     0x172644	/* size = 8Mb */
#define PSX_PAGE_REGS_BASE	  0x1f900000
#define PSX_PAGE_REGS_SIZE	  0x132644	/* size = 512Kb */
#define PSX_PAGE_FLASH_BASE	 0x1f980000
#define PSX_PAGE_FLASH_SIZE	 0x122644	/* !!! size = 256Kb ??? */
#define PSX_PAGE_B_BASE	     0x1fa00000
#define PSX_PAGE_B_SIZE	     0x152644	/* size = 2Mb */
#define PSX_PAGE_C_BASE	     0x1fc00000
#define PSX_PAGE_C_SIZE	     0x162644	/* size = 4Mb */

/* PIO pages switch routines */
#define psx_page_switch(page) {\
        mb ();\
	outl (PSX_PAGE_A_BASE, PIO_PAGE_BASE_PORT);\
	outl (PSX_PAGE_ ## page ## _SIZE, PIO_BUS_PORT);\
	outl (PSX_PAGE_ ## page ## _BASE, PIO_PAGE_BASE_PORT);\
        mb ();\
}

#define psx_page_switch_ret(page,base,size) {\
        mb ();\
	base = inl (PIO_PAGE_BASE_PORT);\
	size = inl (PIO_BUS_PORT);\
	psx_page_switch (page);\
}

#define psx_page_switch_to(base,size) {\
        mb ();\
	outl (PSX_PAGE_A_BASE, PIO_PAGE_BASE_PORT);\
	outl (size, PIO_BUS_PORT);\
	outl (base, PIO_PAGE_BASE_PORT);\
        mb ();\
}

#endif
