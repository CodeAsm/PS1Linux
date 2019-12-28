/* blkmem.c: Block access to memory spaces
 *
 * Copyright (C) 2000  Lineo, Inc.  (www.lineo.com)   
 * Copyright (C) 1997, 1998  D. Jeff Dionne <jeff@lineo.ca>,
 *                           Kenneth Albanowski <kjahds@kjahds.com>,
 *
 * Based z2ram - Amiga pseudo-driver to access 16bit-RAM in ZorroII space
 * Copyright (C) 1994 by Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 *
 * NOV/2000 -- hacked for Linux kernel 2.2 and NETtel/x86 (gerg@lineo.com)
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/malloc.h>
#include <linux/reboot.h>
#include <linux/ledman.h>
#include <linux/init.h>
#include <linux/tqueue.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#undef VERBOSE
#undef DEBUG

#define	BLKMEM_MAJOR	31

#define MAJOR_NR BLKMEM_MAJOR

// #define DEVICE_NAME "blkmem"
#define DEVICE_REQUEST do_blkmem_request
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM
#define TIMEOUT_VALUE (6 * HZ)

#include "blkmem.h"

#include <linux/blk.h>

#ifdef CONFIG_LEDMAN
#include <linux/ledman.h>
#endif

#include <asm/bitops.h>
#include <asm/delay.h>
#include <asm/semaphore.h>

#ifdef CONFIG_COLDFIRE
/*
 *	The ROMfs sits after the kernel bss segment.
 */
unsigned char *romarray;
extern char _ebss;
#endif

#define	CONFIG_NETtel	1
char blkmem_buf[0x10000];

#define TRUE                  (1)
#define FALSE                 (0)

/******* END OF BOARD-SPECIFIC CONFIGURATION ************/

/* Simple romfs, at internal, cat on the end of kernel, or seperate fixed adderess romfs. */

#ifdef INTERNAL_ROMARRAY
#include "testromfs.c"
#endif

#if defined(CONFIG_WATCHDOG)
extern void watchdog_disable(void);
extern void watchdog_enable(void);
#endif

#ifdef FIXED_ROMARRAY
unsigned char *romarray = (char *)(FIXED_ROMARRAY);
#endif

/* If defined, ROOT_ARENA causes the root device to be the specified arena, useful with romfs */

/* Now defining ROOT_DEV in arch/setup.c */
/*#define ROOT_ARENA 0*/

struct arena_t;

typedef void (*xfer_func_t)(struct arena_t *, unsigned long address, unsigned long length, char * buffer);
typedef void (*erase_func_t)(struct arena_t *, unsigned long address);
typedef void (*program_func_t)(struct arena_t *, struct blkmem_program_t * prog);

#if defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || defined(CONFIG_DISKtel)
void flash_writeall(struct arena_t *, struct blkmem_program_t *);
void flash_write(struct arena_t *, unsigned long, unsigned long, char *);
void flash_erase(struct arena_t *, unsigned long);
void flash_eraseconfig(void);
#endif

#ifdef CONFIG_COLDFIRE
extern unsigned long _ramstart;
#endif

/* This array of structures defines the actual set of memory arenas, including
   access functions (if the memory isn't part of the main address space) */

struct arena_t {
	int rw;
	
	unsigned long address; /* Address of memory arena */
	unsigned long length;  /* Length of memory arena. If -1, try to get size from romfs header */
	
	program_func_t program_func; /* Function to program in one go */
	
	xfer_func_t read_func; /* Function to transfer data to main memory, or zero if none needed */
	xfer_func_t write_func; /* Function to transfer data from main memory, zero if none needed */
		
	erase_func_t erase_func; /* Function to erase a block of memory to zeros, or 0 if N/A */
	unsigned long blksize; /* Size of block that can be erased at one time, or 0 if N/A */
	unsigned long unitsize;
	unsigned char erasevalue; /* Contents of sectors when erased */
	
	/*unsigned int auto_erase_bits;
	unsigned int did_erase_bits;*/
	
} arena[] = {

#ifdef INTERNAL_ROMARRAY
	{romarray, sizeof(romarray)},
#endif

#ifdef CONFIG_COLDFIRE
/*
 *	The ROM file-system is RAM resident on the ColdFire eval boards.
 *	This arena is defined for access to it.
 */
    {0, 0, -1},
#define FIXUP_ARENAS 	arena[0].address = (unsigned long) &_ebss; \
						arena[0].length = _ramstart - arena[0].address;
						

#ifdef CONFIG_M5206
/*
 *	The spare FLASH segment on the 5206 board.
 */
    {1,0xffe20000,0x20000,0,0,write_pair,erase_pair,0x8000,0x20000,0xff},
#endif

#ifdef CONFIG_CADRE3
/*  pair of AM29LV004T flash for 1Mbyte total
 *  rom0 -- root file-system (actually in RAM)
 *  rom1 -- FLASH SA0   128K boot
 *  rom2 -- FLASH SA1-6 768k kernel & romfs
 *  rom3 -- FLASH SA7   64k spare
 *  rom4 -- FLASH SA8   16k spare
 *  rom5 -- FLASH SA9   16k spare
 *  rom6 -- FLASH SA10  32k spare
 */ 
    {1,0xffe00000,0x20000,0,0,write_pair,erase_pair,0x20000,0x20000,0xff},
    {1,0xffe20000,0xc0000,0,0,write_pair,erase_pair,0x20000,0xc0000,0xff},
    {1,0xffee0000,0x10000,0,0,write_pair,erase_pair,0x10000,0x10000,0xff},
    {1,0xffef0000,0x4000,0,0,write_pair,erase_pair,0x4000,0x4000,0xff},
    {1,0xffef4000,0x4000,0,0,write_pair,erase_pair,0x4000,0x4000,0xff},
    {1,0xffef8000,0x8000,0,0,write_pair,erase_pair,0x8000,0x8000,0xff},
#endif /* CONFIG_CADRE3 */

#if defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || defined(CONFIG_DISKtel)
#if defined(CONFIG_FLASH2MB) || defined(CONFIG_FLASH4MB)
/*
 *	NETtel with 2MB/4MB FLASH erase/program entry points.
 *	The following devices are supported:
 *		rom0 -- root file-system (actually in RAM)
 *		rom1 -- FLASH boot block (16k)
 *		rom2 -- FLASH boot arguments (8k)
 *		rom3 -- FLASH MAC addresses (8k)
 *		rom4 -- FLASH kernel+file-system binary (1920k)
 *		rom5 -- FLASH config file-system (64k)
 *		rom6 -- FLASH the whole damn thing (2Mb)!
 *		rom7 -- FLASH spare block (32k)
 *		rom8 -- FLASH2 kernel+file-system binary (1920k) (4MB only)
 *    rom9 -- FLASH2 the whole damn thing (2Mb)!
 */
    {1,0xf0000000,0x004000,flash_writeall, 0, 0, 0,    0x04000,0x004000,0xff},
    {1,0xf0004000,0x002000,0,0,flash_write,flash_erase,0x02000,0x002000,0xff},
    {1,0xf0006000,0x002000,0,0,flash_write,flash_erase,0x02000,0x002000,0xff},
    {1,0xf0020000,0x1e0000,flash_writeall, 0, 0, 0,    0x10000,0x1e0000,0xff},
    {1,0xf0010000,0x010000,0,0,flash_write,flash_erase,0x10000,0x010000,0xff},
    {1,0xf0000000,0x200000,flash_writeall, 0, 0, 0,    0x10000,0x200000,0xff},
    {1,0xf0008000,0x08000,flash_writeall,0,flash_write,flash_erase,0x08000,0x08000,0xff},
#if defined(CONFIG_FLASH4MB)
    {1,0xf0220000,0x1e0000,flash_writeall, 0, 0, 0,    0x10000,0x1e0000,0xff},
    {1,0xf0200000,0x200000,flash_writeall, 0, 0, 0,    0x10000,0x200000,0xff},
#endif
#else
/*
 *	NETtel FLASH erase/program entry points.
 *	The following devices are supported:
 *		rom0 -- root file-system (actually in RAM)
 *		rom1 -- FLASH boot block (16k)
 *		rom2 -- FLASH boot arguments (8k)
 *		rom3 -- FLASH MAC addresses (8k)
 *		rom4 -- FLASH kernel+file-system binary (896k)
 *		rom5 -- FLASH config file-system (64k)
 *		rom6 -- FLASH the whole damn thing (1Mb)!
 *		rom7 -- FLASH spare block (32k)
 */
    {1,0xf0000000,0x04000,flash_writeall, 0, 0, 0,    0x04000,0x04000,0xff},
    {1,0xf0004000,0x02000,0,0,flash_write,flash_erase,0x02000,0x02000,0xff},
    {1,0xf0006000,0x02000,0,0,flash_write,flash_erase,0x02000,0x02000,0xff},
    {1,0xf0010000,0xe0000,flash_writeall, 0, 0, 0,    0x10000,0xe0000,0xff},
    {1,0xf00f0000,0x10000,0,0,flash_write,flash_erase,0x10000,0x10000,0xff},
    {1,0xf0000000,0x100000,flash_writeall, 0, 0, 0,  0x10000,0x100000,0xff},
    {1,0xf0008000,0x08000,flash_writeall,0,flash_write,flash_erase,0x08000,0x08000,0xff},
#if defined(CONFIG_EXTRA_FLASH1MB)
/*
 *		rom8 -- FLASH extra. where the NETtel3540 stores the dsl image
 */
    {1,0xf0100000,0x100000,flash_writeall, 0, 0, 0,  0x10000,0x100000,0xff},
#endif /*CONFIG_EXTRA_FLASH1MB*/
#endif /* CONFIG_FLASH2MB */
#endif /* CONFIG_NETtel || CONFIG_eLIA || CONFIG_DISKtel */

#endif /* CONFIG_COLDFIRE */

#ifdef CONFIG_NETTEL_X86
#ifdef CONFIG_FLASH16MB
    {1,0x04000000,0x0e0000,flash_writeall, 0, 0, 0,     0x20000,0x0e0000,0xff},
    {1,0x04100000,0xf00000,flash_writeall, 0, 0, 0,     0x20000,0xf00000,0xff},
    {1,0x040e0000,0x020000,0,0,flash_write, flash_erase,0x20000,0x020000,0xff},
    {1,0x04000000,0x1000000,flash_writeall, 0, 0, 0,    0x20000,0x1000000,0xff},
#else
    {1,0x04000000,0x0e0000,flash_writeall, 0, 0, 0,     0x20000,0x0e0000,0xff},
    {1,0x04100000,0x700000,flash_writeall, 0, 0, 0,     0x20000,0x700000,0xff},
    {1,0x040e0000,0x020000,0,0,flash_write, flash_erase,0x20000,0x020000,0xff},
    {1,0x04000000,0x800000,flash_writeall, 0, 0, 0,     0x20000,0x800000,0xff},
#endif
#endif /* CONFIG_NETTEL_X86 */

};

#define arenas (sizeof(arena) / sizeof(struct arena_t))


static int blkmem_blocksizes[arenas];
static int blkmem_sizes[arenas];


#if defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || defined(CONFIG_DISKtel)

static DECLARE_MUTEX(spare_lock);

/*
 *	FLASH erase routine for the 29LV800 part on the NETtel board.
 */

void flash_erase(struct arena_t *a, unsigned long pos)
{
  unsigned volatile char *address;
  unsigned long fbase = a->address;
  unsigned long flags;
  unsigned char status;
  int i;
  
#if 0
  printk("%s(%d): flash_erase(a=%x,pos=%x)\n", __FILE__, __LINE__,
    (int) a, (int) pos);
#endif

  if (pos >= a->length)
    return;

  address = (volatile unsigned char *) (fbase + pos);

  /* Mutex all access to FLASH memory */
  down(&spare_lock);
  save_flags(flags); cli();

#if defined(CONFIG_WATCHDOG)
  watchdog_disable();
#endif

  /* Erase this sector */
  *address = 0x20;
  *address = 0xd0;

  for (i = 0; (i < 10000000); i++) {
    status = *address;
    if (status & 0x80)
      break;
  }

  /* Restore FLASH to normal read mode */
  *address = 0xff;

  if (*address != 0xff) {
     printk("FLASH: (%d): erase failed, address %p iteration=%d "
		"status=%x\n", __LINE__, address, i, (int) status);
  }

#if defined(CONFIG_WATCHDOG)
  watchdog_enable();
#endif

  restore_flags(flags);
  up(&spare_lock);
}

/*
 *	Erase only the configuration file system...
 *	Used to support the software reset button.
 */
void flash_eraseconfig(void)
{
	flash_erase(arena + 2, 0);
}


/*
 *	FLASH programming routine for the NETtel board.
 */

void flash_write(struct arena_t * a, unsigned long pos, unsigned long length, char * buffer)
{
  unsigned long		flags, ptr, min, max;
  unsigned char		*lp, *lp0, status;
  int			failures, j, k, l;
  
#if 0
  printk("%s(%d): flash_write(a=%x,pos=%x,length=%d,buf=%x)\n",
	__FILE__, __LINE__, (int) a, (int) pos, (int) length, (int) buffer);
#endif

  down(&spare_lock);

#if 0
  /* Buffer should already be in kernel space... */
  if (copy_from_user(&blkmem_buf[0], buffer, length))
	return;
  buffer = &blkmem_buf[0];
#endif

#if defined(CONFIG_WATCHDOG)
  watchdog_disable();
#endif

  min = (a->address + pos);
  max = min + length;;
  lp = (unsigned char *) buffer;

  for (ptr = min; (ptr < max); ptr += l) {

      save_flags(flags); cli();

      /* Determine write size */
      lp0 = lp;
      j = max - ptr;
      l = (j < 32) ? j : 32;
      if ((ptr & ~0x1f) != ptr) {
	j = 32 - (ptr & 0x1f);
	l = (l < j) ? l : j;
      }

      /* Program next buffer bytes */
      for (j = 0; (j < 16000000); j++) {
	*((volatile unsigned char *) ptr) = 0xe8;
	status = *((volatile unsigned char *) ptr);
	if (status & 0x80)
	  break;
      }
      if ((status & 0x80) == 0)
	goto writealldone;

      *((volatile unsigned char *) ptr) = (l-1);
      for (j = 0; (j < l); j++)
	*((volatile unsigned char *) (ptr+j)) = *lp++;

      *((volatile unsigned char *) ptr) = 0xd0;

      for (j = 0; (j < 16000000); j++) {
	status = *((volatile unsigned char *) ptr);
	if (status & 0x80) {
	  /* Program complete */
	  break;
	}
      }

writealldone:
      /* Restore FLASH to normal read mode */
      *((volatile unsigned char *) ptr) = 0xff;

      for (k = 0; (k < l); k++, lp0++) {
      	status = *((volatile unsigned char *) (ptr+k));
        if (status != *lp0) {
		printk("FLASH: (%d): write failed, addr=%08x wrote=%02x "
		"read=%02x cnt=%d len=%d\n",
		__LINE__, (int) (ptr+k), (int) *lp0, (int) status, j, l);
		failures++;
	}
      }

      restore_flags(flags);
    }

#if defined(CONFIG_WATCHDOG)
  watchdog_enable();
#endif

  up(&spare_lock);
}


#if 0
/*
 *	FLASH programming routine for the NETtel board.
 */
void flash_writeslow(struct arena_t * a, unsigned long pos, unsigned long length, char * buffer)
{
  volatile unsigned char *address;
  unsigned long flags, fbase = a->address;
  unsigned char *lbuf, status;
  int i;
  
#if 0
  printk("%s(%d): flash_write(a=%x,pos=%x,length=%d,buf=%x)\n",
	__FILE__, __LINE__, (int) a, (int) pos, (int) length, (int) buffer);
#endif

  down(&spare_lock);

#if 0
  /* Buffer should already be in kernel space... */
  if (copy_from_user(&blkmem_buf[0], buffer, length))
	return;
  buffer = &blkmem_buf[0];
#endif

#if defined(CONFIG_WATCHDOG)
  watchdog_disable();
#endif

  address = (unsigned volatile char *) (fbase + pos);
  lbuf = (unsigned char *) buffer;

  for (; (length > 0); length--, address++, lbuf++) {
  
    if (*address != *lbuf) {
      save_flags(flags); cli();

      *address = 0x40;
      *address = *lbuf;

      for (i = 0; (i < 0x1000000); i++) {
	status = *address;
	if (status & 0x80) {
	  /* Program complete */
	  break;
	}
      }

      /* Restore FLASH to normal read mode */
      *address = 0xff;

      if (*address != *lbuf) {
          printk("FLASH: (%d): write failed i=%d, address %p -> %x(%x)\n",
		__LINE__, i, address, (int) *lbuf, (int) *address);
      }

      restore_flags(flags);
    }
  }

#if defined(CONFIG_WATCHDOG)
  watchdog_enable();
#endif

  up(&spare_lock);
}
#endif


/*
 *	Program a complete FLASH image. This runs from DRAM, so no
 *	need to worry about writing to what we are running from...
 */

void flash_writeall(struct arena_t * a, struct blkmem_program_t * prog)
{
  unsigned long		erased[16];
  unsigned long		base, offset, ptr, min, max;
  unsigned char		*lp, *lp0, status;
  int			failures, i, j, k, l;

#if defined(CONFIG_WATCHDOG)
  watchdog_disable();
#endif
  
  printk("FLASH: programming");
  failures = 0;
  memset(&erased[0], 0, sizeof(erased));

  cli();
  
  for (i = 0; (i < prog->blocks); i++) {

#if 0
    printk("%s(%d): block=%d address=%x pos=%x length=%x range=%x-%x\n",
      __FILE__, __LINE__, i, (int) a->address, (int) prog->block[i].pos,
      (int) prog->block[i].length, (int) (prog->block[i].pos + a->address),
      (int) (prog->block[i].pos + prog->block[i].length - 1 + a->address));
#endif

#if 0
    if (prog->block[i].length > 0x100000)
      break;
#endif

    /*
     *	Erase FLASH sectors for this program block...
     */
    for (l = prog->block[i].pos / a->blksize;
	l <= ((prog->block[i].pos+prog->block[i].length-1) / a->blksize);
	l++) {

      if (erased[(l / 32)] & (0x1 << (l % 32)))
	continue;

      ptr = l * a->blksize;
      offset = ptr % a->unitsize;
      base = ptr - offset;
	
      base += a->address;
      ptr += a->address;
      j = 0;

#if 0
      printk("%s(%d): ERASE BLOCK sector=%d ptr=%x\n",
	  __FILE__, __LINE__, l, (int) ptr);
#endif

      /* Erase this sector */
      *((volatile unsigned char *) ptr) = 0x20;
      *((volatile unsigned char *) ptr) = 0xd0;

#define	FTIMEOUT	16000000

      for (k = 0; (k < FTIMEOUT); k++) {
	status = *((volatile unsigned char *) ptr);
	if (status & 0x80) {
	  /* Erase complete */
	  status = *((volatile unsigned char *) ptr);
	  break;
	}
      }

      /* Restore FLASH to normal read mode */
      *((volatile unsigned char *) ptr) = 0xff;

      if (k >= FTIMEOUT) {
	  printk("FLASH: (%d) erase failed, status=%08x\n",
		__LINE__, (int) status);
	  failures++;
	  /* Continue (with unerased sector) */
	  break;
      }

      erased[(l / 32)] |= (0x1 << (l % 32));
    }

    /*
     *	Program FLASH with the block data...
     */
    min = prog->block[i].pos+a->address;
    max = prog->block[i].pos+prog->block[i].length+a->address;
    lp = (unsigned char *) prog->block[i].data;

    if (copy_from_user(&blkmem_buf[0], prog->block[i].data, prog->block[i].length)) {
	printk("FLASH: failed to get user buffers\n");
	return;
    }
    lp = (unsigned char *) &blkmem_buf[0];

    /* Progress indicators... */
    printk(".");
#ifdef CONFIG_LEDMAN
    ledman_cmd(LEDMAN_CMD_OFF, (i & 1) ? LEDMAN_NVRAM_1 : LEDMAN_NVRAM_2);
    ledman_cmd(LEDMAN_CMD_ON, (i & 1) ? LEDMAN_NVRAM_2 : LEDMAN_NVRAM_1);
#endif

#if 0
    printk("%s(%d): PROGRAM BLOCK min=%x max=%x\n", __FILE__, __LINE__,
      (int) min, (int) max);
#endif


    for (ptr = min; (ptr < max); ptr += l) {

      /* Determine write size */
      lp0 = lp;
      j = max - ptr;
      l = (j < 32) ? j : 32;
      if ((ptr & ~0x1f) != ptr) {
	j = 32 - (ptr & 0x1f);
	l = (l < j) ? l : j;
      }

#if 0
      printk("%s(%d): PROGRAM ptr=%x l=%d\n", __FILE__, __LINE__, (int) ptr, l);
#endif

      /* Program next buffer bytes */
      for (j = 0; (j < 16000000); j++) {
	*((volatile unsigned char *) ptr) = 0xe8;
	status = *((volatile unsigned char *) ptr);
	if (status & 0x80)
	  break;
      }
      if ((status & 0x80) == 0)
	goto writealldone;

      *((volatile unsigned char *) ptr) = (l-1);
      for (j = 0; (j < l); j++)
	*((volatile unsigned char *) (ptr+j)) = *lp++;

      *((volatile unsigned char *) ptr) = 0xd0;

      for (j = 0; (j < 16000000); j++) {
	status = *((volatile unsigned char *) ptr);
	if (status & 0x80) {
	  /* Program complete */
	  break;
	}
      }

writealldone:
      /* Restore FLASH to normal read mode */
      *((volatile unsigned char *) ptr) = 0xff;

      for (k = 0; (k < l); k++, lp0++) {
      	status = *((volatile unsigned char *) (ptr+k));
        if (status != *lp0) {
		printk("FLASH: (%d): write failed, addr=%08x wrote=%02x "
		"read=%02x cnt=%d len=%d\n",
		__LINE__, (int) (ptr+k), (int) *lp0, (int) status, j, l);
		failures++;
	}
      }
    }
  }

  if (failures > 0) {
    printk("FLASH: %d failures programming FLASH!\n", failures);
    return;
  }

  printk("\nFLASH: programming successful!\n");
  if (prog->reset) {
    printk("FLASH: rebooting...\n\n");
    machine_restart(NULL);
  }
}

#endif /* CONFIG_NETtel || CONFIG_eLIA || CONFIG_DISKtel */


int general_program_func(struct inode * inode, struct file * file, struct arena_t * a, struct blkmem_program_t * prog)
{
  int i,block;
  unsigned int erased = 0;

  /* Mandatory flush of all dirty buffers */
  fsync_dev(inode->i_rdev);
  invalidate_buffers(inode->i_rdev);

  for(i=0;i<prog->blocks;i++) {
    int min= prog->block[i].pos / a->blksize;
    int max = (prog->block[i].pos + prog->block[i].length - 1) / a->blksize;
    for(block=min; block <= max; block++) {
      if (!test_bit(block, &erased)) {
	printk("Erase of sector at pos %lx of arena %d (address %p)\n", block * a->blksize, MINOR(inode->i_rdev), (void*)(a->address+block*a->blksize));
    
        /* Invoke erase function */
        a->erase_func(a, block * a->blksize);
        set_bit(block, &erased);
      }
    }
    
    printk("Write of %lu bytes at pos %lu (data at %p)\n", prog->block[i].length, prog->block[i].pos, prog->block[i].data);
    
    a->write_func(a, prog->block[i].pos, prog->block[i].length, prog->block[i].data);
    
    schedule();
  }

#ifdef CONFIG_UCLINUX
  if (prog->reset)
  	HARD_RESET_NOW();
#endif
  return 0;
}


static void complete_request(void * data);

static struct tq_struct complete_tq = {
		routine: complete_request,
		data: NULL
};

static void
delay_request( void )
{
  schedule_task(&complete_tq);
}

static void
complete_request( void * data)
{
  unsigned long start;
  unsigned long len;
  struct arena_t * a = arena + DEVICE_NR(CURRENT_DEV);

  for(;;) {
  
  /* sectors are 512 bytes */
  start = CURRENT->sector << 9;
  len  = CURRENT->current_nr_sectors << 9;

  /*printk("blkmem: re-request %d\n", CURRENT->cmd);*/
    
  if ( CURRENT->cmd == READ ) {
    /*printk("BMre-Read: %lx:%lx > %p\n", a->address + start, len, * CURRENT->buffer);*/
    a->read_func(a, start, len, CURRENT->buffer);
  }
  else if (CURRENT->cmd == WRITE) {
    /*printk("BMre-Write: %p > %lx:%lx\n", CURRENT->buffer, a->address + start, len);*/
    a->write_func(a, start, len, CURRENT->buffer);
  }
  /*printk("ending blkmem request\n");*/
  end_request( TRUE );
  
  INIT_REQUEST;
  }
  
#if 0
  if (CURRENT)
    do_blkmem_request(); /* Process subsequent requests */
#endif
}


static void
do_blkmem_request(request_queue_t *q)
{
  unsigned long start;
  unsigned long len;
  struct arena_t * a = arena + DEVICE_NR(CURRENT_DEV);
  struct request *current = CURRENT;

#if 0
  printk( KERN_ERR DEVICE_NAME ": request\n");
#endif

  while ( TRUE ) {
    INIT_REQUEST;
    
    /* sectors are 512 bytes */
    start = current->sector << 9;
    len  = current->current_nr_sectors << 9;

    if ((start + len) > a->length) {
      printk( KERN_ERR DEVICE_NAME ": bad access: block=%ld, count=%ld (pos=%lx, len=%lx)\n",
	      current->sector,
	      current->current_nr_sectors,
	      start+len,
	      a->length);
      end_request( FALSE );
      continue;
    }

    /*printk("blkmem: request %d\n", current->cmd);*/
    
    if ( ( current->cmd != READ ) 
	 && ( current->cmd != WRITE ) 
	 ) {
      printk( KERN_ERR DEVICE_NAME ": bad command: %d\n", current->cmd );
      end_request( FALSE );
      continue;
    }
    
    if ( current->cmd == READ ) {
      if (a->read_func) {
//printk("%s,%d: READ delay_request\n", __FILE__, __LINE__);
        delay_request();
        return;
        /*a->read_func(a, start, len, current->buffer);*/
      }
      else {
//printk("%s,%d: memcpy(0x%x, 0x%x, %d)\n", __FILE__, __LINE__, current->buffer,(void*)(a->address+start), len);
        memcpy( current->buffer, (void*)(a->address + start), len );
      }
    }
    else if (current->cmd == WRITE) {
      if (a->write_func) {
//printk("%s,%d: WRITE delay_request\n", __FILE__, __LINE__);
        delay_request();
        return;
        /*a->write_func(a, start, len, current->buffer);*/
      } else {
//printk("%s,%d: memcpy(0x%x, 0x%x, %d)\n", __FILE__, __LINE__, (void*)(a->address + start), current->buffer, len );
        memcpy( (void*)(a->address + start), current->buffer, len );
	  }
    }
    /* printk("ending blkmem request\n"); */
    end_request( TRUE );
  }
}


static int blkmem_ioctl (struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg)
{
  struct arena_t * a = arena + MINOR(inode->i_rdev);

  switch (cmd) {

  case BMGETSIZES:   /* Return device size in sectors */
    if (!arg)
      return -EINVAL;
    return(put_user(a->blksize ? (a->length / a->blksize) : 0, (unsigned long *) arg));
    break;

  case BMGETSIZEB:   /* Return device size in bytes */
    return(put_user(a->length, (unsigned long *) arg));
    break;

  case BMSERASE:
    if (a->erase_func) {

      if (arg >= a->length)
        return -EINVAL;
    
      /* Mandatory flush of all dirty buffers */
      fsync_dev(inode->i_rdev);
      
      /* Invoke erase function */
      a->erase_func(a, arg);
    } else
      return -EINVAL;
    break;
    
  case BMSGSIZE:
    return(put_user(a->blksize, (unsigned long*)arg));
    break;

  case BMSGERASEVALUE:
    return(put_user(a->erasevalue, (unsigned char *) arg));
    break;

  case BMPROGRAM:
  {
    struct blkmem_program_t * prog;
    int i;

    prog = (struct blkmem_program_t *) kmalloc(16000, GFP_KERNEL);

    if (prog == NULL)
	return(-ENOMEM);

    if (copy_from_user(prog, (void *) arg, 16000))
	return(-EFAULT);

    if ((prog->magic1 != BMPROGRAM_MAGIC_1) ||
        (prog->magic2 != BMPROGRAM_MAGIC_2))
      return -EINVAL;

    for(i=0;i<prog->blocks;i++) {
      if(prog->block[i].magic3 != BMPROGRAM_MAGIC_3)
        return -EINVAL;
    }

    for(i=0;i<prog->blocks;i++)
      if ((prog->block[i].pos > a->length) ||
          ((prog->block[i].pos+prog->block[i].length-1) > a->length))
        return -EINVAL;

    if (a->program_func) {
      a->program_func(a, prog);
    } else {
      return general_program_func(inode, file, a, prog);
    }

    break;
  }

  default:
    return -EINVAL;
  }
  return 0;
}

static int
blkmem_open( struct inode *inode, struct file *filp )
{
  int device;
  struct arena_t * a;

  device = DEVICE_NR( inode->i_rdev );

#if 0
  printk( KERN_ERR DEVICE_NAME ": open: %d\n", device );
#endif

  if ((MINOR(device) < 0) || (MINOR(device) >= arenas))  {
    printk("arena open of %d failed!\n", MINOR(device));
    return -ENODEV;
  }
  
  a = &arena[MINOR(device)];

#if defined(MODULE)
  MOD_INC_USE_COUNT;
#endif
  return 0;
}

static int 
blkmem_release( struct inode *inode, struct file *filp )
{
#if 0
  printk( KERN_ERR DEVICE_NAME ": release: %d\n", current_device );
#endif

  fsync_dev( inode->i_rdev );

#if defined(MODULE)
  MOD_DEC_USE_COUNT;
#endif

  return(0);
}

#if DAVIDM
static struct file_operations blkmem_fops =
{
  NULL,                   /* lseek - default */
  block_read,             /* read - general block-dev read */
  block_write,            /* write - general block-dev write */
  NULL,                   /* readdir - bad */
  NULL,                   /* poll */
  blkmem_ioctl,           /* ioctl */
  NULL,
  blkmem_open,            /* open */
  NULL,                   /* flush */
  blkmem_release,         /* release */
  block_fsync,            /* fsync */
  NULL,                   /* fasync */
  NULL,                   /* check media change */
  NULL,                   /* revalidate */
};

#endif

static struct block_device_operations blkmem_fops=
{
	open:		blkmem_open,
	release:	blkmem_release,
	ioctl:		blkmem_ioctl,
};


int __init blkmem_init( void )
{
  int i;
  unsigned long realaddrs[arenas];

#ifdef FIXUP_ARENAS
  {
  FIXUP_ARENAS
  }
#endif

  for(i=0;i<arenas;i++) {
    if (arena[i].length == -1) {
#if 1
      arena[i].length = 8 * 1024 * 1024; /* FIXME: should calculate size */
#else
      unsigned long len;
      len = *(volatile unsigned long *)(arena[i].address + 8);
      arena[i].length = ntohl(len);
#endif
    }
    blkmem_blocksizes[i] = 1024;
    blkmem_sizes[i] = (arena[i].length + (1 << 10) - 1) >> 10; /* Round up */
    arena[i].length = blkmem_sizes[i] << 10;

    realaddrs[i] = arena[i].address;
    arena[i].address = (unsigned long) ioremap_nocache((int) arena[i].address, arena[i].length);
  }


  printk("Blkmem copyright 1998,1999 D. Jeff Dionne\nBlkmem copyright 1998 Kenneth Albanowski\nBlkmem %d disk images:\n", (int) arenas);

  for(i=0;i<arenas;i++) {
    printk("%d: %lX-%lX [VIRTUAL %lX-%lX] (%s)\n", i,
	realaddrs[i], realaddrs[i]+arena[i].length-1,
	arena[i].address, arena[i].address+arena[i].length-1,
    	arena[i].rw 
    		? "RW"
    		: "RO"
    );
  }

  if (register_blkdev(MAJOR_NR, DEVICE_NAME, &blkmem_fops )) {
    printk( KERN_ERR DEVICE_NAME ": Unable to get major %d\n",
            MAJOR_NR );
    return -EBUSY;
  }

  blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), &do_blkmem_request);

  read_ahead[ MAJOR_NR ] = 0;
  blksize_size[ MAJOR_NR ] = blkmem_blocksizes;
  blk_size[ MAJOR_NR ] = blkmem_sizes;
  
#ifdef ROOT_ARENA
  ROOT_DEV = MKDEV(MAJOR_NR,ROOT_ARENA);
#endif
#if !defined(MODULE)
#ifndef CONFIG_COLDFIRE
  /*if (MAJOR(ROOT_DEV) == FLOPPY_MAJOR) {*/
    ROOT_DEV = MKDEV(MAJOR_NR,1);
  /*}*/
#endif
#endif
  return 0;
}

static void __exit
blkmem_exit(void )
{
  int i;

  for(i=0;i<arenas;i++)
      iounmap((void *) arena[i].address);

  if ( unregister_blkdev( MAJOR_NR, DEVICE_NAME ) != 0 )
    printk( KERN_ERR DEVICE_NAME ": unregister of device failed\n");
}

EXPORT_NO_SYMBOLS;

module_init(blkmem_init);
module_exit(blkmem_exit);

