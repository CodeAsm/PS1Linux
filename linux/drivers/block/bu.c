/*
** bu - PlayStation memory card block driver.
*/

#include <linux/config.h>

#ifdef CONFIG_PSX_LARGE_CARD
#define MAJOR_NR	BU_LARGE_MAJOR
#else
#define MAJOR_NR   BU_MAJOR
#endif

#include <linux/major.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/blkpg.h>

#include <asm/io.h>
#include <asm/delay.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/ps/interrupts.h>

#include "bu.h"

#define TRUE                  (1)
#define FALSE                 (0)

#define SECTOR_SZ_SHIFT       (9)

#define TIMEOUT_VALUE         (100)
#define CATCH_TIMEOUT         (100)
#define N_CHECKS              (10)

#undef CALCULATE_FLOOR
#define MYSTERIOUS_DELAY
#define MYSTERIOUS_DELAY_VALUE	1000

static int bu_hardsects[BU_MINORS];
static int bu_blocksizes[BU_MINORS];
static int bu_sizes[BU_MINORS];
static volatile int bu_total = 0;
static bu_device_t bu_devices[BU_MINORS];
static int bu_current = -1;                // current card number
static volatile int bu_state = BU_NONE;   // current transfer state
static volatile int bu_lock = FALSE;      // driver locked flag
static volatile int bu_step = 0;          // current request step
static volatile int bu_open = FALSE;
static volatile int bu_try = 0;
static volatile int bu_continue = FALSE; // request function called from
                                            //interrupt handler or timeout handler
static struct timer_list bu_timer;
static bu_request_t bu_curr_request = {0, 0, 0};
static bu_t bu_curr = {&bu_curr_request};
static DECLARE_WAIT_QUEUE_HEAD (bu_wait);

static int bu_rd_routine (bu_t *bu);
#define BU_DELAY(a) udelay(a)

static int bu_catch (int card, int checks, int timeout) {
   int i, flags;
   
   for (i = 0; i < checks; i++) {
	   save_flags(flags);
	   cli();
      if (bu_lock) {
         restore_flags (flags);
#ifdef DEBUG
   printk (KERN_INFO "bu_catch: card %d try lock goto sleep\n", card+1);   
#endif   
         sleep_on_timeout (&bu_wait, timeout);
#ifdef DEBUG
   printk (KERN_INFO "bu_catch: card %d try lock\n", card+1);   
#endif   
      }
      else {
         bu_lock = TRUE;
         restore_flags (flags);
#ifdef DEBUG
   printk (KERN_INFO "bu_catch: card %d catched\n", card+1);   
#endif   
         return 1;
      }
   }
#ifdef DEBUG
   printk (KERN_ERR "bu_catch: can't catch card %d\n", card+1);
#endif   
   return 0;
}

static void bu_interrupt (int irq, void * dev_id, struct pt_regs * regs) {
   int status;
   unsigned long flags;

   if (bu_current < 0) {
#ifdef DEBUG
      printk (KERN_ERR "bu_interrupt: no current device\n");
#endif
      return;
   }

   if (bu_open) {   
      bu_state = BU_READY;
      wake_up (&bu_wait);
   }
   else {
		status = bu_rd_routine (&bu_curr);

 		if (status < 0) {
         del_timer (&bu_timer);
#ifdef DEBUG
         printk ("bu_interrupt: operation filed\n");
#endif
         spin_lock_irqsave (&io_request_lock, flags);
         bu_continue = TRUE;
         do_bu_request (NULL);
         bu_continue = FALSE;
         spin_unlock_irqrestore (&io_request_lock, flags);
         return;
		}
      		
	   if (bu_curr.stop) {
         del_timer (&bu_timer);
         if (CURRENT->cmd == READ) {
            memcpy (CURRENT->buffer+(bu_step << BU_BLK_SHIFT), bu_curr_request.buffer, BU_BLK_SIZE);
         }
         bu_step++;
         bu_try = 0;
         spin_lock_irqsave (&io_request_lock, flags);
         bu_continue = TRUE;
         do_bu_request (NULL);
         bu_continue = FALSE;
         spin_unlock_irqrestore (&io_request_lock, flags);
         return;
      }
      else {
         mod_timer (&bu_timer, jiffies+bu_devices[bu_curr_request.card].timeout);
         return;
      }
   }
}

static void bu_timeout (unsigned long card) {
   unsigned long flags;
   
   if (bu_current < 0) {
#ifdef DEBUG
      printk (KERN_ERR "bu_timeout: no current device\n");
#endif
      return;
   }
   
   if (bu_open) {   
      bu_state = BU_TIMEOUT;
      wake_up (&bu_wait);
   }
   else {
      del_timer (&bu_timer);
#ifdef DEBUG
      printk ("bu_timeout: operation timeout\n");
#endif
      spin_lock_irqsave (&io_request_lock, flags);
      bu_continue = TRUE;
      do_bu_request (NULL);
      bu_continue = FALSE;
      spin_unlock_irqrestore (&io_request_lock, flags);
      return;
   }
}

static int bu_ready (int card) {
   bu_state = BU_WAIT;
   
   init_timer (&bu_timer);
   bu_timer.function = bu_timeout;
   bu_timer.data = NULL;
   bu_timer.expires = jiffies+bu_devices[card].timeout;
   add_timer (&bu_timer);
   // check - may be we lose interrupt ?
   if (bu_state != BU_READY) {
      sleep_on (&bu_wait);
   }
   del_timer (&bu_timer);
   if (bu_state == BU_READY) return 0;
   else return -1;
}

static void bu_hw_init (void) {
	outw (0x40, BU_CONTROL);
	outb (0x88, BU_REGE);
	outb (0x0d, BU_REG8);
	outw (0x00, BU_CONTROL);
	BU_DELAY (10);
	outw (0x02, BU_CONTROL);
	BU_DELAY (10);
	outw (0x2002, BU_CONTROL);
	BU_DELAY (10);
	outw (0x00, BU_CONTROL);
}

static int bu_sw_init (bu_t *bu) {

	bu->cnt = 0;
	bu->stop = 0;

#ifdef CALCULATE_FLOOR
	bu->bu_request->floor = (bu->bu_request->block >> BU_FLOOR_SHIFT) & BU_FLOOR_MASK;  
	bu->bu_request->block = bu->bu_request->block & BU_BLOCK_MASK;
#endif

	bu->cs = ((bu->bu_request->block) & 0xff) ^ (((bu->bu_request->block) >> 8) & 0xff);
   if (bu->bu_request->mode == 'R')
		bu->state = 0;
	else if (bu->bu_request->mode == 'W')
		bu->state = 0x10;
   else
 		return -1; // unknown mode

	return 0;
}

static int bu_rd_state0 (bu_t *bu) {

	outw ((((bu->bu_request->card) & 1) << 13) | 0x1003, BU_CONTROL);
	bu->byte = inb (BU_DATA);
	outb( 0x81+bu->bu_request->floor, BU_DATA );
#if 0	
	if (bu->bu_request->floor > 7)
		outb (0x81-bu->bu_request->floor, BU_DATA);
	else
		outb (0x81+bu->bu_request->floor, BU_DATA);
#endif
	return 1;
}

static int bu_rd_state1 (bu_t *bu) {
	bu->byte = inb (BU_DATA);
	outb (bu->bu_request->mode, BU_DATA);
	return 1;
}

static int bu_rd_state2(bu_t *bu) {
	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
   bu->hw_state = bu->byte;
	return 1;
}

static int bu_rd_state3 (bu_t *bu) {
	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
   if (bu->byte == 0x5a) return 1;
	return -1;
}

static int bu_rd_state4 (bu_t *bu) {
	bu->byte = inb (BU_DATA);
	outb (((bu->bu_request->block) >> 8) & 0xff, BU_DATA);
   if (bu->byte == 0x5d) return 1;
	return -1;
}

static int bu_rd_state5 (bu_t *bu) {
	bu->byte = inb (BU_DATA);
	outb ((bu->bu_request->block)&0xff, BU_DATA);
   return 1;
}

static int bu_rd_state6 (bu_t *bu) {
//  outw (0x1003, BU_CONTROL);
	bu->byte = inb (BU_DATA); // high byte of block address
	outb(0, BU_DATA);
   return 1;
}

static int bu_rd_state7 (bu_t *bu) {
//  outw (0x803, BU_CONTROL);
	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
   if (bu->byte == 0x5c) return 1;
	return -1;
}

static int bu_rd_state8 (bu_t *bu)
{
	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
   if (bu->byte == 0x5d) return 1;
	return -1;
}

static int bu_rd_state9 (bu_t *bu) {
//  outw (0x803, BU_CONTROL);
	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
   if (bu->byte == ((bu->bu_request->block >> 8) & 0xff)) return 1;
	return -1;
}

static int bu_rd_state10 (bu_t *bu) {
	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
   if (bu->byte == (bu->bu_request->block & 0xff)) return 1;
	return -1;
}

static int bu_rd_state12 (bu_t *bu) {
	int timeout = 0;

	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
	if (bu->bu_request->mode == 'R')
	   if (bu->byte != bu->cs) return -1;

   while ((inw (BU_STATUS) & 7) != 7)
		if (timeout++ > 10000) return -1;

	bu->byte = inb (BU_DATA);
	outb (0, BU_DATA);
   if (bu->byte == 0x47) {
		bu->stop = 1;
		return 1;
	}
	return -1;
}

static int bu_wr_state7 (bu_t *bu) {
//	inb (BU_DATA);
	inb (BU_DATA);
	outb (bu->cs, BU_DATA);
	return 1;
}

static int bu_rd_data (bu_t *bu) {
	__u8 byte = inb (BU_DATA);
	bu->bu_request->buffer[bu->cnt] = byte;
	bu->cs ^= byte;
	outb (0x00, BU_DATA);
	bu->cnt++;
	return ((bu->cnt == BU_BLK_SIZE) ? 1 : 0);
}

static int bu_wr_data (bu_t *bu) {
	__u8 byte = bu->bu_request->buffer[bu->cnt]; 
	bu->cs ^= byte;
	inb (BU_DATA);
	outb (byte, BU_DATA);
	bu->cnt++;
	return ((bu->cnt == BU_BLK_SIZE) ? 1 : 0);
}

int ((*bu_rd_states[])(bu_t*)) = {
	bu_rd_state0, bu_rd_state1, bu_rd_state2, bu_rd_state3, bu_rd_state4,
	bu_rd_state5, bu_rd_state6, bu_rd_state7, bu_rd_state8, bu_rd_state9,
	bu_rd_state10, bu_rd_data, bu_rd_state12, NULL,					NULL,
	NULL,
	bu_rd_state0, bu_rd_state1, bu_rd_state2, bu_rd_state3, bu_rd_state4,
	bu_rd_state5, bu_wr_data, bu_wr_state7,	bu_rd_state6,  	bu_rd_state7,
	bu_rd_state12
};	

int bu_rd_routine (bu_t *bu) {
	int i;
	int status;
   
#ifdef MYSTERIOUS_DELAY
	for(i = 0; i < MYSTERIOUS_DELAY_VALUE; i++);
#endif	
	outw (inw (BU_CONTROL) | 0x13 | (((bu->bu_request->card) & 1) << 13), BU_CONTROL);
	status = (bu_rd_states[bu->state]) (bu);
	//outw (inw (BU_CONTROL) | 0x10, BU_CONTROL);
	if (status<0) return -1; //error
	bu->state += status;
#if 0
	if (bu->stop == 0)
	; // end of packet transmission
#endif
	return 0; //ok
}

static void do_bu_request (request_queue_t * q) {
   static u_long start, len;
	static int card;
   int status = 0;
#ifdef CONFIG_PSX_LARGE_CARD
	int j;
#endif

   if (!bu_continue) {
start: 
		if (bu_lock) return;
      
      while (TRUE)
      {
	      INIT_REQUEST;
      
         card = DEVICE_NR (CURRENT->rq_dev);
#ifdef CONFIG_PSX_LARGE_CARD
         if (card > 0) {
#else
         if (card >= BU_MINORS) {
#endif
	         printk (KERN_ERR DEVICE_NAME ": request for unknown device: %d\n", card+1);
	         end_request (FALSE);
	         continue;
	      }
      
         start = CURRENT->sector << (SECTOR_SZ_SHIFT-BU_BLK_SHIFT);
         len = CURRENT->current_nr_sectors << (SECTOR_SZ_SHIFT-BU_BLK_SHIFT);
#ifndef CONFIG_PSX_LARGE_CARD
		   bu_total = bu_devices[card].first_block.size-BU_FIRST_BLOCKS;
#endif

#ifdef DEBUG
	      printk (KERN_INFO DEVICE_NAME ": request for %d card, start=%ld(%ld), len=%ld(%ld), total=%d\n",
		      card+1, CURRENT->sector, start, CURRENT->current_nr_sectors, len, bu_total);
#endif

	      if ((start+len) > bu_total) {
	         printk (KERN_ERR DEVICE_NAME ": bad access: block=%ld, count=%ld\n",
		         CURRENT->sector,
		         CURRENT->current_nr_sectors);
	         end_request (FALSE);
	         continue;
	      }

	      if (CURRENT->cmd == READ) {
            bu_curr_request.mode = 'R';
         }
         else if (CURRENT->cmd == WRITE) {
            bu_curr_request.mode = 'W';
         }
         else {
	         printk (KERN_ERR DEVICE_NAME ": bad command: %d\n", CURRENT->cmd);
	         end_request (FALSE);
	         continue;
	      }

#ifndef CONFIG_PSX_LARGE_CARD
         bu_curr_request.card = card;
#endif
	      
         bu_lock = TRUE;
         bu_current = card;
         bu_step = 0;
         bu_try = 0;
         break;
      }
   }
   
   if (bu_step < len) {
      if (bu_try >= N_CHECKS) {
	      printk (KERN_ERR DEVICE_NAME ": block operation for card %d filed: err=0x%x,"
            "hw_status=0x%x,byte=0x%x\n", bu_curr_request.card+1,
            bu_curr.state, bu_curr.hw_state, (int)bu_curr.byte);
	      end_request (FALSE);
		   bu_current = -1;
		   bu_lock = FALSE;
			if (! QUEUE_EMPTY)
				goto start;
			else return;
      }
      else bu_try++;
      
      // fill low-level request data
      if (CURRENT->cmd == WRITE) {
         memcpy (bu_curr_request.buffer, CURRENT->buffer+(bu_step << BU_BLK_SHIFT), BU_BLK_SIZE);
      }
      bu_curr_request.block = start+bu_step;
#ifdef CONFIG_PSX_LARGE_CARD
		for (j = 0; j < BU_MINORS; j++) {
			if (bu_devices[j].first_block.size > 0) {
				bu_curr_request.block += BU_FIRST_BLOCKS;
				if (bu_curr_request.block < bu_devices[j].first_block.size) break;
				bu_curr_request.block -= bu_devices[j].first_block.size;
			}
		}
      if (j >= BU_MINORS) {
      	printk (KERN_ERR DEVICE_NAME ": bad card number found: %d\n",
      		j+1);
         end_request (FALSE);
			bu_current = -1;   
			bu_lock = FALSE;
			if (! QUEUE_EMPTY)
				goto start;
			else return;
      }
		bu_curr_request.card = j;
#else
		bu_curr_request.block += BU_FIRST_BLOCKS;
#endif
      bu_curr_request.floor = 0x0;

#ifdef DEBUG
	   printk (KERN_INFO DEVICE_NAME ": block operation card=%d, op=%c, block=%d\n",
		   bu_curr_request.card+1, bu_curr_request.mode, bu_curr_request.block);
#endif

      // init one block operation
      bu_state = BU_NONE;
         
      if ((status = bu_sw_init (&bu_curr)) < 0) {
	      printk (KERN_ERR DEVICE_NAME ": block operation for card %d filed: err=0x%x,"
            "status=0x%x,hw_status=0x%x,byte=0x%x\n", bu_curr_request.card+1, status,
            bu_curr.state, bu_curr.hw_state, (int)bu_curr.byte);
	      end_request (FALSE);
		   bu_current = -1;
		   bu_lock = FALSE;
			if (! QUEUE_EMPTY)
				goto start;
			else return;
      }
      
      bu_hw_init ();

	   status = bu_rd_routine (&bu_curr);

 	  if (status < 0) {
	      printk (KERN_ERR DEVICE_NAME ": block operation for card %d filed: err=0x%x,"
            "status=0x%x,hw_status=0x%x,byte=0x%x\n", bu_curr_request.card+1, status,
            bu_curr.state, bu_curr.hw_state, (int)bu_curr.byte);
	      end_request (FALSE);
		   bu_current = -1;
		   bu_lock = FALSE;
			if (! QUEUE_EMPTY)
				goto start;
			else return;
      }
   
   	init_timer (&bu_timer);
   	bu_timer.function = bu_timeout;
   	bu_timer.data = NULL;
   	bu_timer.expires = jiffies+bu_devices[bu_curr_request.card].timeout;
   	add_timer (&bu_timer);
      
      return;
   }
   else {
#ifdef DEBUG
	   printk (KERN_INFO DEVICE_NAME ": request for %d card done\n", card+1);
#endif
	   end_request (TRUE);
		bu_current = -1;
		bu_lock = FALSE;
		if (! QUEUE_EMPTY)
			goto start;
		else return;
   }
}

static int bu_check (bu_t * bu) {
	int status;
   int try = 0;

try:
   if (try >= N_CHECKS) {
   	return 0;
   }
   else try++;

   if (bu_sw_init (bu) < 0) return 0;
   
   bu_hw_init ();
   
	do
	{
      if ((bu->state & 0xf) != 0) {
         if (bu_ready (bu->bu_request->card) < 0) {
#ifdef DEBUG
          printk (KERN_ERR DEVICE_NAME ": check operation for card %d timeout: st=0x%x,"
               "status=0x%x,hw_status=0x%x,byte=0x%x,control=0x%x,count=%d\n", 
              bu->bu_request->card+1, inw (BU_STATUS),
              bu->state, bu->hw_state, (int)bu->byte, inw (BU_CONTROL), bu->cnt);
#endif
            goto try;
	      }
      }

	   status = bu_rd_routine (bu);

 	   if (status < 0) {
#ifdef DEBUG
       printk (KERN_ERR DEVICE_NAME ": check operation for card %d filed: err=0x%x,"
            "status=0x%x,hw_status=0x%x,byte=0x%x\n", bu->bu_request->card+1, status,
            bu->state, bu->hw_state, (int)bu->byte);
#endif
         goto try;
	   }		
	} while (!bu->stop);
   
   return 1;
}

static int bu_read_first_block (int card) {
	bu_t bu = {0};
   bu_request_t bu_request = {0, 0, 0};
   union {
      __u8  fill[128];
      bu_first_block_t block;
   } first_block;

	bu_request.block = 0x0;
	bu_request.mode = 'R';
	bu_request.card = card;
   bu_request.floor = 0x0;
	bu.bu_request = &bu_request;
	if (!bu_check (&bu)) {
		// the block is unreadable, no blocks on the floor 
#ifdef DEBUG
      printk (KERN_ERR DEVICE_NAME ": can't read %d card first block\n", card+1);
#endif
		return 0;
	}
	
	memcpy (first_block.fill, bu_request.buffer, BU_BLK_SIZE);
	
	if (first_block.block.id != BU_ID) {
		// bad card id - card wasn't write properly
#ifdef DEBUG
      printk (KERN_ERR DEVICE_NAME ": bad card id: 0x%x\n", first_block.block.id);
#endif
		return 0;
	}
	
	bu_devices[card].first_block.id = first_block.block.id;
	bu_devices[card].first_block.size = first_block.block.size;
	bu_devices[card].first_block.serial = first_block.block.serial;
	bu_devices[card].first_block.number = first_block.block.number;
	
	return 1;
}

static int bu_do_open (struct inode *inode, struct file *filp) {
   int card = DEVICE_NR (inode->i_rdev);

#ifdef DEBUG   
	printk ("try to open %d card (curr=%d, lock=%d)\n", card+1, bu_current, bu_lock);
#endif

#ifdef CONFIG_PSX_LARGE_CARD
	if (card > 0) return -ENODEV;
#else
   if (card >= BU_MINORS) return -ENODEV;
#endif
	
   if (!bu_devices[card].usage) {
      check_disk_change (inode->i_rdev);
      if (bu_sizes[card] == 0) {
         return -ENXIO;
      }
   }
   bu_devices[card].usage++;
      
   MOD_INC_USE_COUNT;

#ifdef DEBUG   
	printk ("%d card opened\n", card+1);
#endif
	
   return 0;
}

static int bu_release (struct inode *inode, struct file *filp) {
   int card = DEVICE_NR (inode->i_rdev);

#ifdef DEBUG   
	printk ("try to release %d card (curr=%d, lock=%d)\n", card+1, bu_current, bu_lock);
#endif

   bu_devices[card].usage--;
   
   MOD_DEC_USE_COUNT;

   return 0;
}

static int bu_check_change (kdev_t dev) {

#ifdef DEBUG   
	printk ("try to check card change\n");
#endif

#ifdef CONFIG_PSX_LARGE_CARD
   if (MINOR (dev) > 0) return 0;
#else
   if (MINOR (dev) >= BU_MINORS) return 0;
#endif
   
   return 1;
}

static int bu_revalidate (kdev_t dev) {
   int card = DEVICE_NR (dev);
   int i, bu_size = 0;
#ifdef CONFIG_PSX_LARGE_CARD
	int n = 0;
#endif

#ifdef DEBUG   
	printk ("try to revalidate %d card (curr=%d, lock=%d)\n", card+1, bu_current, bu_lock);
#endif

   if (!bu_catch (card, N_CHECKS, CATCH_TIMEOUT)) {
#ifdef DEBUG   
      printk (KERN_ERR DEVICE_NAME ": can't lock card %d: device busy\n", card);
#endif
      return -EBUSY;
   }
   bu_current = card;
   bu_state = BU_NONE;
   bu_open = TRUE;
   
   bu_total = 0;
#ifdef CONFIG_PSX_LARGE_CARD
	for (i = 0, n = 0; i < BU_MINORS; i++) {
#else
	{
		i = card;
#endif
		if (!bu_read_first_block (i)) {    
         printk (KERN_INFO DEVICE_NAME ": card in slot %d not found\n", i+1);
         bu_sizes[i] = 0;
      }
      else {
#ifdef CONFIG_PSX_LARGE_CARD
			if (bu_devices[i].first_block.number != n) {
	   		printk (KERN_ERR DEVICE_NAME ": Bad card sequence - found %d instead %d\n",
	   			bu_devices[i].first_block.number, n);
            bu_sizes[card] = 0;   
   			bu_current = -1;   
   			bu_open = FALSE;
   			bu_lock = FALSE;
      		return -ENXIO;
			}
			else {
#endif	
      	bu_sizes[i] = (bu_devices[i].first_block.size >> (10-BU_BLK_SHIFT))-1;
			bu_size += bu_sizes[i];
			bu_total += bu_devices[i].first_block.size-BU_FIRST_BLOCKS;
         printk (KERN_INFO DEVICE_NAME ": %d Kbytes card found in slot %d\n", 
         	bu_sizes[i], i+1);
#ifdef CONFIG_PSX_LARGE_CARD
			}
         n++;
#endif	
      }   
	}
      
#ifdef CONFIG_PSX_LARGE_CARD
	bu_sizes[0] = bu_size;
#endif	

   bu_current = -1;   
   bu_open = FALSE;
   bu_lock = FALSE;
	
	if (bu_size == 0) {
      return -ENODEV;
   }
   
   return 0;
}

static int bu_ioctl (struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg) {
	int err;
	long size;
	struct hd_geometry geo;

#ifdef DEBUG
	printk ("bu_ioctl: cmd=%d\n", cmd);
#endif
	
	switch (cmd) {
		case BLKGETSIZE:
			if (!arg) return -EINVAL;
			err = !access_ok (VERIFY_WRITE, arg, sizeof (long));
			if (err) return -EFAULT;
			size = bu_sizes[MINOR (inode->i_rdev)]*1024/bu_hardsects[MINOR (inode->i_rdev)];
			if (copy_to_user ((long *)arg, &size, sizeof (long))) return -EFAULT;
			return 0;
			
		case BLKRRPART:
			return -ENOTTY;
			
		case HDIO_GETGEO:
			err = !access_ok (VERIFY_WRITE, arg, sizeof (geo));
			if (err) return -EFAULT;
			size = bu_sizes[MINOR (inode->i_rdev)]*1024/bu_hardsects[MINOR (inode->i_rdev)];
			geo.cylinders = (size & ~0x3f) >> 6;
			geo.heads = 4;
			geo.sectors = 16;
			geo.start = 4;
			if (copy_to_user ((void *)arg, &geo, sizeof (geo))) return -EFAULT;
			return 0;
			
		default:
			return blk_ioctl (inode->i_rdev, cmd, arg);
	}
	
	return -ENOTTY;
}

static struct block_device_operations bu_fops =
{
	open:		bu_do_open,
	release:	bu_release,
	ioctl:	  bu_ioctl,
   check_media_change: bu_check_change,
   revalidate: bu_revalidate,
};

int __init bu_init (void) {
   int i, bu_size = 0, n = 0;

#ifdef CONFIG_PSX_LARGE_CARD
   if (register_blkdev (MAJOR_NR, "bul", &bu_fops)) {
#else
   if (register_blkdev (MAJOR_NR, "bu", &bu_fops)) {
#endif
	   printk (KERN_ERR DEVICE_NAME ": Unable to get major %d\n",
	      MAJOR_NR);
	   return -EBUSY;
   }
   
   for (i = 0; i < BU_MINORS; i++) {
      bu_hardsects[i] = BU_HARDSECSIZE;
      bu_blocksizes[i] = BU_BSIZE;
      bu_sizes[i] = 0;
      bu_devices[i].timeout = TIMEOUT_VALUE;
   }

   blk_init_queue (BLK_DEFAULT_QUEUE (MAJOR_NR), DEVICE_REQUEST);
   blk_size[MAJOR_NR] = bu_sizes;
   blksize_size[MAJOR_NR] = bu_blocksizes;
   hardsect_size[MAJOR_NR] = bu_hardsects;
   read_ahead[MAJOR_NR] = BU_RAHEAD;

   i = request_irq (CONTROLLER, bu_interrupt, SA_INTERRUPT, DEVICE_NAME, NULL);
   if (i < 0) {
      printk (KERN_ERR DEVICE_NAME ": can't get irq %d\n", CONTROLLER);

      if (unregister_blkdev (MAJOR_NR, DEVICE_NAME) != 0)
         printk (KERN_ERR DEVICE_NAME ": unregister of device failed\n");
      
      blk_cleanup_queue (BLK_DEFAULT_QUEUE(MAJOR_NR));
      
      blk_size[MAJOR_NR] = NULL;
      blksize_size[MAJOR_NR] = NULL;
      hardsect_size[MAJOR_NR] = NULL;
      read_ahead[MAJOR_NR] = 0;
         
      return i;
   }
   
   // check of card existence
   for (i = 0, bu_total = 0, n = 0; i < BU_MINORS; i++) {
      printk (KERN_INFO DEVICE_NAME ": detecting card in slot %d ...\n", i+1);
      if (!bu_catch (i, N_CHECKS, CATCH_TIMEOUT)) {
         printk (KERN_ERR DEVICE_NAME ": can't catch card in slot %d\n", i+1);
         continue;
      }
      bu_current = i;
      bu_state = BU_NONE;
      bu_open = TRUE;
		if (!bu_read_first_block (i)) {    
         printk (KERN_INFO DEVICE_NAME ": card in slot %d not found\n", i+1);
         bu_sizes[i] = 0;
      }
      else {
#ifdef CONFIG_PSX_LARGE_CARD
			if (bu_devices[i].first_block.number != n) {
	   		printk (KERN_ERR DEVICE_NAME ": Bad card sequence - found %d instead %d\n",
	   			bu_devices[i].first_block.number, n);
			}
			else {
#endif	
      	bu_sizes[i] = (bu_devices[i].first_block.size >> (10-BU_BLK_SHIFT))-1;
			bu_size += bu_sizes[i];
			bu_total += bu_devices[i].first_block.size-BU_FIRST_BLOCKS;
         printk (KERN_INFO DEVICE_NAME ": %d Kbytes card found in slot %d\n", 
         	bu_sizes[i], i+1);
#ifdef CONFIG_PSX_LARGE_CARD
			}
#endif	
         n++;
      }
      bu_current = -1;
      bu_open = FALSE;
      bu_lock = FALSE;
   }

#ifdef CONFIG_PSX_LARGE_CARD
	bu_sizes[0] = bu_size;
   printk (KERN_INFO DEVICE_NAME ": driver initialized: %d cards joined, total size = %d Kbytes\n",
      BU_MINORS, bu_size);
#else
   printk (KERN_INFO DEVICE_NAME ": driver for %d cards initialized\n",
      BU_MINORS);
#endif

   return 0;
}

#if defined(MODULE)
int init_module (void) {
   int error;

   error = bu_init ();
   if (error == 0)
   {
      printk (KERN_INFO DEVICE_NAME ": loaded as module\n");
   }

   return error;
}

void cleanup_module (void)
{
   int i;
   
#ifdef CONFIG_PSX_LARGE_CARD
   fsync_dev (MKDEV (MAJOR_NR, 0));
#else
   for (i = 0; i < BU_MINORS; i++)
      fsync_dev (MKDEV (MAJOR_NR, i));
#endif

   free_irq (CONTROLLER, NULL);

   if (unregister_blkdev (MAJOR_NR, DEVICE_NAME) != 0)
      printk (KERN_ERR DEVICE_NAME ": unregister of device failed\n");

   blk_cleanup_queue (BLK_DEFAULT_QUEUE(MAJOR_NR));
      
   blk_size[MAJOR_NR] = NULL;
   blksize_size[MAJOR_NR] = NULL;
   hardsect_size[MAJOR_NR] = NULL;
   read_ahead[MAJOR_NR] = 0;

   return;
} 
#endif
