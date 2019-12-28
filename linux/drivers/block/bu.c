/*
** bu - PlayStation memory card block driver.
*/

#define MAJOR_NR    BU_MAJOR

#include <linux/major.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <asm/io.h>
#include <asm/delay.h>
#include <asm/system.h>
#include <asm/ps/interrupts.h>

#include "bu.h"

#define TRUE                  (1)
#define FALSE                 (0)

#define SECTOR_SZ_SHIFT       (9)

#define TIMEOUT_VALUE         (10)
#define N_CHECKS              (10)

static int bu_blocksizes[BU_MINORS];
static int bu_sizes[BU_MINORS];
static bu_device bu_devices[BU_MINORS];
static int bu_current = -1;
static DECLARE_WAIT_QUEUE_HEAD (bu_wait);
static struct timer_list bu_timer;

extern unsigned long jiffies;

#define BU_DELAY(a) udelay(a)

static __volatile__ int bu_catch (int card, int checks, int timeout) {
   DECLARE_WAIT_QUEUE_HEAD (bu_catch_wait);
   int i;
   
   for (i = 0; i < checks; i++) {
      if (bu_devices[card].in_use) {
         sleep_on_timeout (&bu_catch_wait, timeout);
      }
      else {
         bu_devices[card].in_use = 1;
         return 1;
      }
   }
   
#ifdef DEBUG
   printk (KERN_ERR "bu_catch: can't catch card %d\n", card+1);   
#endif   
   
   return 0;
}

static void bu_interrupt (int irq, void * dev_id, struct pt_regs * regs) {
   if (bu_current < 0) {
#ifdef DEBUG
      printk (KERN_ERR "bu_interrupt: no current device\n");
#endif
      return;
   }
   
   bu_devices[bu_current].state = BU_READY;
   del_timer (&bu_timer);
   wake_up (&bu_wait);
}

static void bu_timeout (unsigned long card) {
   bu_devices[card].state = BU_TIMEOUT;
   wake_up (&bu_wait);
}

static __volatile__ int bu_ready (int card) {
   bu_devices[card].state = BU_WAIT;
   init_timer (&bu_timer);
   bu_timer.function = bu_timeout;
   bu_timer.data = card;
   bu_timer.expires = jiffies+bu_devices[card].timeout;
   add_timer (&bu_timer);
   // check - may be we lose interrupt ?
   if (bu_devices[card].state == BU_READY) {
      del_timer (&bu_timer);
   }
   else {
      sleep_on (&bu_wait);
   }
   if (bu_devices[card].state == BU_READY) return 0;
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

	bu->bu_request->floor = (bu->bu_request->block >> BU_FLOOR_SHIFT) & BU_FLOOR_MASK;  
	bu->bu_request->block = bu->bu_request->block & BU_BLOCK_MASK;

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
	if (bu->bu_request->floor > 7)
		outb (0x81-bu->bu_request->floor, BU_DATA);
	else
		outb (0x81+bu->bu_request->floor, BU_DATA);
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
	int status;
   
	outw (inw (BU_CONTROL) | 0x13, BU_CONTROL);
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
   u_long start, len;
	bu_t bu = {0};
   bu_request_t bu_request = {0, 0, 0};
	int status = 0, card;
   int i;
   int try;
   
   bu.bu_request = &bu_request;  

   while (TRUE)
   {
	   INIT_REQUEST;
      
      card = DEVICE_NR (CURRENT->rq_dev);
      if (card >= BU_MINORS) {
	      printk (KERN_ERR DEVICE_NAME ": request for unknown device: %d\n", card);
	      end_request (FALSE);
	      continue;
	   }
      
      start = CURRENT->sector << SECTOR_SZ_SHIFT;
      len = CURRENT->current_nr_sectors << SECTOR_SZ_SHIFT;
	   if ((start+len) > BU_SIZE) {
	      printk (KERN_ERR DEVICE_NAME ": bad access: block=%ld, count=%ld\n",
		      CURRENT->sector,
		      CURRENT->current_nr_sectors);
	      end_request (FALSE);
	      continue;
	   }

	   if (CURRENT->cmd == READ) {
         bu_request.mode = 'R';
      }
      else if (CURRENT->cmd == WRITE) {
         bu_request.mode = 'W';
      }
      else {
	      printk (KERN_ERR DEVICE_NAME ": bad command: %d\n", CURRENT->cmd);
	      end_request (FALSE);
	      continue;
	   }

      bu_request.card = card;
      
      start >>= BU_BLK_SHIFT;       // convert to bu-blocks
      len >>= BU_BLK_SHIFT;

      for (i = 0; i < len; i++) {
         // fill low-level request data
         if (CURRENT->cmd == WRITE) {
            memcpy (bu_request.buffer, CURRENT->buffer+(i << BU_BLK_SHIFT), BU_BLK_SIZE);
         }
         bu_request.block = start+i;
         bu_request.floor = 0x0;

         // init one block operation
         if (!bu_catch (card, N_CHECKS, bu_devices[card].timeout)) {
            end_request (FALSE);
          goto filed;
         }
         bu_current = card;
         bu_devices[card].state = BU_NONE;
         
         try = 0;
try:     if (try >= N_CHECKS) {
            bu_current = -1;
            bu_devices[card].in_use = 0;
	         end_request (FALSE);
	         printk (KERN_ERR DEVICE_NAME ": block operation filed: err=0x%x,"
               "status=0x%x,hw_status=0x%x,byte=0x%x\n", status,
               bu.state, bu.hw_state, (int)bu.byte);
            goto filed;
         }
         else try++;
         
         if ((status = bu_sw_init (&bu)) < 0) {
	         printk (KERN_ERR DEVICE_NAME ": block operation filed: 0x%x\n", status);
            bu_current = -1;
            bu_devices[card].in_use = 0;
	         end_request (FALSE);
	         goto try;
         }
         // one block operation
         
         bu_hw_init ();
         
	      do {
            if ((bu.state & 0xf) != 0) {
               if (bu_ready (card) < 0) {
 	               goto try;
		         }
            }

		      status = bu_rd_routine (&bu);

 		      if (status < 0) {
	            goto try;
		      }		
	      } while (!bu.stop);
         
         if (CURRENT->cmd == READ) {
            memcpy (CURRENT->buffer+(i << BU_BLK_SHIFT), bu_request.buffer, BU_BLK_SIZE);
         }
         bu_current = -1;
         bu_devices[card].in_use = 0;
      }
      
	   end_request (TRUE);
filed:
   }
}

static int bu_check (int card) {
	bu_t bu = {0};
   bu_request_t bu_request = {0, 0, 0};
	int status;
   int try = 0;

try:
   if (try >= N_CHECKS) return 0;
   else try++;

   bu.bu_request = &bu_request;  
   bu_request.mode = 'R';
   bu_request.block = 0x0;
   bu_request.floor = 0x0;
   bu_request.card = card;
   if (bu_sw_init( &bu ) < 0) return 0;
   
   bu_hw_init ();
   
	do
	{
      if ((bu.state & 0xf) != 0) {
         if (bu_ready (card) < 0) {
#ifdef DEBUG
          printk (KERN_ERR DEVICE_NAME ": block operation timeout: st=0x%x,"
               "status=0x%x,hw_status=0x%x,byte=0x%x,control=0x%x\n", inw (BU_STATUS),
              bu.state, bu.hw_state, (int)bu.byte, inw (BU_CONTROL));
#endif
            goto try;
	      }
      }

	   status = bu_rd_routine (&bu);

 	   if (status < 0) {
#ifdef DEBUG
       printk (KERN_ERR DEVICE_NAME ": block operation filed: err=0x%x,"
            "status=0x%x,hw_status=0x%x,byte=0x%x\n", status,
            bu.state, bu.hw_state, (int)bu.byte);
#endif
         goto try;
	   }		
	} while (!bu.stop);
   
   return 1;
}

static int bu_open (struct inode *inode, struct file *filp) {
   int card = DEVICE_NR (inode->i_rdev);

   if (card >= BU_MINORS) return -ENXIO;

   if (!bu_catch (card, N_CHECKS, bu_devices[card].timeout)) {
#ifdef DEBUG   
      printk ("bu_open: can't catch card %d\n", card);
#endif
      return -ENXIO;
   }
   bu_current = card;
   bu_devices[card].state = BU_NONE;
   
   if (!bu_check (card)) {
      bu_current = -1;
      bu_devices[card].in_use = 0;
      return -ENXIO;
   }
   
   MOD_INC_USE_COUNT;

   bu_current = -1;   
   bu_devices[card].in_use = 0;
   return 0;
}

static int bu_release (struct inode *inode, struct file *filp) {

   MOD_DEC_USE_COUNT;

   return 0;
}

static int bu_check_change (kdev_t dev) {

   if (MINOR (dev) >= BU_MINORS) return 0;
   
   return 1;
}

static int bu_revalidate (kdev_t dev) {
   return 0;
}

static struct block_device_operations bu_fops =
{
	open:		bu_open,
	release:	bu_release,
   check_media_change: bu_check_change,
   revalidate: bu_revalidate,
};

int __init bu_init (void) {
   int i;

   if (register_blkdev (MAJOR_NR, "bu", &bu_fops)) {
	   printk (KERN_ERR DEVICE_NAME ": Unable to get major %d\n",
	      MAJOR_NR);
	   return -EBUSY;
   }
   
   for (i = 0; i < BU_MINORS; i++) {
      bu_blocksizes[i] = 1024;
      bu_sizes[i] = BU_SIZE >> 10;
      bu_devices[i].in_use = 0;
      bu_devices[i].state = BU_NONE;
      bu_devices[i].timeout = TIMEOUT_VALUE;
   }

   blk_init_queue (BLK_DEFAULT_QUEUE (MAJOR_NR), DEVICE_REQUEST);
   blksize_size[MAJOR_NR] = bu_blocksizes;
   blk_size[MAJOR_NR] = bu_sizes;

   i = request_irq (CONTROLLER, bu_interrupt, SA_INTERRUPT, DEVICE_NAME, NULL);
   if (i < 0) {
      printk (KERN_INFO DEVICE_NAME ": can't get irq %d\n", CONTROLLER);

      if (unregister_blkdev (MAJOR_NR, DEVICE_NAME) != 0)
         printk (KERN_ERR DEVICE_NAME ": unregister of device failed\n");
      
      blk_cleanup_queue (BLK_DEFAULT_QUEUE(MAJOR_NR));
         
      return i;
   }

   printk (KERN_INFO DEVICE_NAME ": driver initialized: %d cards of %dK size %d blocksize\n",
      BU_MINORS, bu_sizes[0], bu_blocksizes[0]);

#ifdef DEBUG
   // check of card existence
   for (i = 0; i < BU_MINORS; i++) {
      if (!bu_catch (i, N_CHECKS, bu_devices[i].timeout)) {
         printk (KERN_INFO DEVICE_NAME ": can't catch card in slot %d\n", i+1);
         continue;
      }
      bu_current = i;
      bu_devices[i].state = BU_NONE;
      if (bu_check (i)) {
         printk (KERN_INFO DEVICE_NAME ": in slot %d found\n", i+1);
      }
      else {
         printk (KERN_INFO DEVICE_NAME ": in slot %d not found\n", i+1);
      }
      bu_current = -1;
      bu_devices[i].in_use = 0;
   }
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
   int i, j;

   free_irq (CONTROLLER, NULL);

   if (unregister_blkdev (MAJOR_NR, DEVICE_NAME) != 0)
      printk (KERN_ERR DEVICE_NAME ": unregister of device failed\n");

   blk_cleanup_queue (BLK_DEFAULT_QUEUE(MAJOR_NR));

   return;
} 
#endif
