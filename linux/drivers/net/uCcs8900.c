/* uCcs89x0.c: A Crystal Semiconductor CS89[02]0 driver for linux. */
/*
	Port for uCsimm 1999-2001 D. Jeff Dionne, Rt-Control Inc. / Lineo, Inc.

	Written 1996 by Russell Nelson, with reference to skeleton.c
	written 1993-1994 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	The author may be reached at nelson@crynwr.com, Crynwr
	Software, 11 Grant St., Potsdam, NY 13676

  Changelog:

  Mike Cruse        : mcruse@cti-ltd.com
                    : Changes for Linux 2.0 compatibility. 
                    : Added dev_id parameter in net_interrupt(),
                    : request_irq() and free_irq(). Just NULL for now.

  Mike Cruse        : Added MOD_INC_USE_COUNT and MOD_DEC_USE_COUNT macros
                    : in net_open() and net_close() so kerneld would know
                    : that the module is in use and wouldn't eject the 
                    : driver prematurely.

  Mike Cruse        : Rewrote init_module() and cleanup_module using 8390.c
                    : as an example. Disabled autoprobing in init_module(),
                    : not a good thing to do to other devices while Linux
                    : is running from all accounts.
*/

static char *version =
"cs89x0.c:v1.02 11/26/96 Russell Nelson <nelson@crynwr.com>\ncs89x0.c:68EZ328 support D. Jeff Dionne <jeff@uclinux.org> 1999\n";

/* ======================= configure the driver here ======================= */


/* use 0 for production, 1 for verification, >2 for debug */
#ifndef NET_DEBUG
#define NET_DEBUG 2
#endif

/* ======================= end of configuration ======================= */


/* Always include 'config.h' first in case the user wants to turn on
   or override something. */
#include <linux/config.h>

#define PRINTK(x) printk x

/*
  Sources:

	Crynwr packet driver epktisa.

	Crystal Semiconductor data sheets.

*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/malloc.h>
#include <linux/string.h>
#include <asm/system.h>
#include <asm/bitops.h>

#ifndef CONFIG_UCCS8900_HW_SWAP
#include <asm/io.h>
#else
#include <asm/io_hw_swap.h>
#endif

#include <asm/irq.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include "uCcs8900.h"

#ifdef CONFIG_M68328
#include <asm/MC68328.h>
#endif

#ifdef CONFIG_M68EZ328
#include <asm/MC68EZ328.h>
#endif

#ifdef CONFIG_UCSIMM
#define NET_BASE 0x10000300
#endif

#ifdef CONFIG_ALMA_ANS
#define NET_BASE 0x10200300
#endif

static unsigned int net_debug = NET_DEBUG;

/* The number of low I/O ports used by the ethercard. */
#define NETCARD_IO_EXTENT	16

void *irq2dev_map[1]; /* FIXME:  This does NOT go here */

/* Information that need to be kept for each board. */
struct net_local {
	struct enet_statistics stats;
	int chip_type;		/* one of: CS8900, CS8920, CS8920M */
	char chip_revision;	/* revision letter of the chip ('A'...) */
	int send_cmd;		/* the propercommand used to send a packet. */
	int auto_neg_cnf;
	int adapter_cnf;
	int isa_config;
	int irq_map;
	int rx_mode;
	int curr_rx_cfg;
        int linectl;
        int send_underrun;      /* keep track of how many underruns in a row we get */
	struct sk_buff *skb;
};

/* Index to functions, as function prototypes. */

extern int cs89x0_probe(struct device *dev);

static int cs89x0_probe1(struct device *dev, int ioaddr);
static int net_open(struct device *dev);
static int	net_send_packet(struct sk_buff *skb, struct device *dev);
static void cs8900_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void set_multicast_list(struct device *dev);
static void net_rx(struct device *dev);
static int net_close(struct device *dev);
static struct enet_statistics *net_get_stats(struct device *dev);
static void reset_chip(struct device *dev);
static int set_mac_address(struct device *dev, void *addr);


/* Example routines you must write ;->. */
#define tx_done(dev) 1


/* Check for a network adaptor of this type, and return '0' iff one exists.
   If dev->base_addr == 0, probe all likely locations.
   If dev->base_addr == 1, always return failure.
   If dev->base_addr == 2, allocate space for the device and return success
   (detachable devices only).
   */

int
cs89x0_probe(struct device *dev)
{
	int base_addr = NET_BASE;

	return cs89x0_probe1(dev, base_addr);
}

int inline
readreg(struct device *dev, int portno)
{
	outw(portno, dev->base_addr + ADD_PORT);
	return inw(dev->base_addr + DATA_PORT);
}

void inline
writereg(struct device *dev, int portno, int value)
{
	outw(portno, dev->base_addr + ADD_PORT);
	outw(value,  dev->base_addr + DATA_PORT);
}

int inline
readword(struct device *dev, int portno)
{
	return inw(dev->base_addr + portno);
}

void inline
writeword(struct device *dev, int portno, int value)
{
	outw(value, dev->base_addr + portno);
}

/* This is the real probe routine.  */

static int cs89x0_probe1(struct device *dev, int ioaddr)
{
	struct net_local *lp;
	static unsigned version_printed = 0;
	int i;
	unsigned rev_type = 0;

	irq2dev_map[0] = dev;

#ifdef CONFIG_UCSIMM
	/* set up the chip select */
	*(volatile unsigned  char *)0xfffff42b |= 0x01; /* output /sleep */
	*(volatile unsigned short *)0xfffff428 |= 0x0101; /* not sleeping */

	*(volatile unsigned  char *)0xfffff42b &= ~0x02; /* input irq5 */
	*(volatile unsigned short *)0xfffff428 &= ~0x0202; /* irq5 fcn on */
	
	*(volatile unsigned short *)0xfffff102 = 0x8000; /* 0x04000000 */
	*(volatile unsigned short *)0xfffff112 = 0x01e1; /* 128k, 2ws, FLASH, en */
#endif

#ifdef CONFIG_ALMA_ANS
        /* 
         * Make sure the chip select (CSA1) is enabled 
	 * Note, that we don't have to program the base address, since
         * it is programmed once for both CSA0 and CSA1 in *-head.S
	 */
        PFSEL &= ~PF_CSA1;
        PFDIR |= PF_CSA1;
 
        /* Make sure that interrupt line (irq3) is enabled too */
        PDSEL  &= ~PD_IRQ3;
        PDDIR  &= ~PD_IRQ3;
        PDKBEN |= PD_IRQ3;                                                       
#endif

	/* Initialize the device structure. */
	if (dev->priv == NULL) {
		dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
                memset(dev->priv, 0, sizeof(struct net_local));
        }
	dev->base_addr = ioaddr;
	lp = (struct net_local *)dev->priv;

	if (readreg(dev, PP_ChipID) != CHIP_EISA_ID_SIG) {
	  printk("cs89x0.c: No CrystalLan device found.\n");
		return ENODEV;
	}

	/* get the chip type */
	rev_type = readreg(dev, PRODUCT_ID_ADD);
	lp->chip_type = rev_type &~ REVISON_BITS;
	lp->chip_revision = ((rev_type & REVISON_BITS) >> 8) + 'A';

	/* Check the chip type and revision in order to set the correct send command
	CS8920 revision C and CS8900 revision F can use the faster send. */
	lp->send_cmd = TX_AFTER_ALL;
#if 0
	if (lp->chip_type == CS8900 && lp->chip_revision >= 'F')
		lp->send_cmd = TX_NOW;
	if (lp->chip_type != CS8900 && lp->chip_revision >= 'C')
		lp->send_cmd = TX_NOW;
#endif
	if (net_debug  &&  version_printed++ == 0)
		printk(version);

	printk("%s: cs89%c0%s rev %c found at 0x%.8x %s",
	       dev->name,
	       lp->chip_type==CS8900?'0':'2',
	       lp->chip_type==CS8920M?"M":"",
	       lp->chip_revision,
	       dev->base_addr,
	       readreg(dev, PP_SelfST) & ACTIVE_33V ? "3.3Volts" : "5Volts");

	reset_chip(dev);

	/* Fill this in, we don't have an EEPROM */
	lp->adapter_cnf = A_CNF_10B_T | A_CNF_MEDIA_10B_T;
	lp->auto_neg_cnf = EE_AUTO_NEG_ENABLE;

	printk(" media %s%s%s",
	       (lp->adapter_cnf & A_CNF_10B_T)?"RJ-45,":"",
	       (lp->adapter_cnf & A_CNF_AUI)?"AUI,":"",
	       (lp->adapter_cnf & A_CNF_10B_2)?"BNC,":"");

	lp->irq_map = 0xffff;

#ifdef CONFIG_ALMA_ANS
	/* Bad hack, has to be fixed, since we have the SEEPROM on board */
	dev->dev_addr[0] = 0x00;
	dev->dev_addr[1] = 0x00;
	dev->dev_addr[2] = 0xc0;
	dev->dev_addr[3] = 0xff;
	dev->dev_addr[4] = 0xee;
	dev->dev_addr[5] = 0x01;
#endif

#ifdef CONFIG_UCSIMM
	{
 		extern unsigned char *cs8900a_hwaddr;
 		memcpy(dev->dev_addr, cs8900a_hwaddr, 6);
	}
#endif

	/* print the ethernet address. */
	for (i = 0; i < ETH_ALEN; i++)
		printk(" %2.2x", dev->dev_addr[i]);

#ifdef FIXME
	/* Grab the region so we can find another board if autoIRQ fails. */
	request_region(ioaddr, NETCARD_IO_EXTENT,"cs89x0");
#endif

	dev->open		= net_open;
	dev->stop		= net_close;
	dev->hard_start_xmit = net_send_packet;
	dev->get_stats	= net_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->set_mac_address = &set_mac_address;

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);

	printk("\n");
	return 0;
}



void
reset_chip(struct device *dev)
{
	int reset_start_time;

	writereg(dev, PP_SelfCTL, readreg(dev, PP_SelfCTL) | POWER_ON_RESET);

	/* wait 30 ms */
	current->state = TASK_INTERRUPTIBLE;
	current->timeout = jiffies + 3;
	schedule();

	/* Wait until the chip is reset */
	reset_start_time = jiffies;
	while( (readreg(dev, PP_SelfST) & INIT_DONE) == 0 && jiffies - reset_start_time < 2)
		;
}

static int
detect_tp(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int timenow = jiffies;

	if (net_debug > 1) printk("%s: Attempting TP\n", dev->name);

        /* If connected to another full duplex capable 10-Base-T card the link pulses
           seem to be lost when the auto detect bit in the LineCTL is set.
           To overcome this the auto detect bit will be cleared whilst testing the
           10-Base-T interface.  This would not be necessary for the sparrow chip but
           is simpler to do it anyway. */
	writereg(dev, PP_LineCTL, lp->linectl &~ AUI_ONLY);

        /* Delay for the hardware to work out if the TP cable is present - 150ms */
	for (timenow = jiffies; jiffies - timenow < 15; )
                ;
	if ((readreg(dev, PP_LineST) & LINK_OK) == 0)
		return 0;

	return A_CNF_MEDIA_10B_T;
}

/* send a test packet - return true if carrier bits are ok */
int
send_test_pkt(struct device *dev)
{
	int ioaddr = dev->base_addr;
	char test_packet[] = { 0,0,0,0,0,0, 0,0,0,0,0,0,
				 0, 46, /* A 46 in network order */
				 0, 0, /* DSAP=0 & SSAP=0 fields */
				 0xf3, 0 /* Control (Test Req + P bit set) */ };
	long timenow = jiffies;
	unsigned short event;

	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) | SERIAL_TX_ON);

	memcpy(test_packet,          dev->dev_addr, ETH_ALEN);
	memcpy(test_packet+ETH_ALEN, dev->dev_addr, ETH_ALEN);
        outw(TX_AFTER_ALL, ioaddr + TX_CMD_PORT);
        outw(ETH_ZLEN, ioaddr + TX_LEN_PORT);

	/* Test to see if the chip has allocated memory for the packet */
	while (jiffies - timenow < 5)
		if (readreg(dev, PP_BusST) & READY_FOR_TX_NOW)
			break;
	if (jiffies - timenow >= 5)
		return 0;	/* this shouldn't happen */

	/* Write the contents of the packet */
	outsw(ioaddr + TX_FRAME_PORT,test_packet,(ETH_ZLEN+1) >>1);

	if (net_debug > 1) printk("Sending test packet ");
	/* wait a couple of jiffies for packet to be received */
	for (timenow = jiffies; jiffies - timenow < 60; )
                ;
        if (((event = readreg(dev, PP_TxEvent)) & TX_SEND_OK_BITS) == TX_OK) {
                if (net_debug > 1) printk("succeeded\n");
                return 1;
        }
	if (net_debug > 1) printk("failed TxEvent 0x%.4x\n",event);
	return 0;
}


void
write_irq(struct device *dev, int chip_type, int irq)
{
  /* we only hooked up 0 :-) */
	writereg(dev, PP_CS8900_ISAINT, 0);
}

/* Open/initialize the board.  This is called (in the current kernel)
   sometime after booting when the 'ifconfig' program is run.

   This routine should set everything up anew at each open, even
   registers that "should" only need to be set once at boot, so that
   there is non-reboot way to recover if something goes wrong.
   */
static int
net_open(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int result = 0;
	int i;

	write_irq(dev, lp->chip_type, 0);

	irq2dev_map[/* FIXME */ 0] = dev;
	writereg(dev, PP_BusCTL, 0); /* ints off! */

#ifdef CONFIG_UCSIMM
	*(volatile unsigned short *)0xfffff302 |= 0x0080; /* +ve pol irq */

        if (request_irq(IRQ_MACHSPEC | IRQ5_IRQ_NUM,
                        cs8900_interrupt,
                        IRQ_FLG_STD,
                        "CrystalLAN_cs8900a", NULL))
                panic("Unable to attach cs8900 intr\n");
#endif

#ifdef CONFIG_ALMA_ANS
	/* We use positive polarity IRQ3 as a network interrupt */
	ICR |= ICR_POL3;

        if (request_irq(IRQ_MACHSPEC | IRQ3_IRQ_NUM,
                        cs8900_interrupt,
                        IRQ_FLG_STD,
                        "CrystalLAN_cs8900a", NULL))
                panic("Unable to attach cs8900 intr\n");                        
#endif

	/* set the Ethernet address */
	set_mac_address(dev, dev->dev_addr);

	/* Set the LineCTL */
	lp->linectl = 0;

        /* check to make sure that they have the "right" hardware available */
	switch(lp->adapter_cnf & A_CNF_MEDIA_TYPE) {
	case A_CNF_MEDIA_10B_T:
		result = lp->adapter_cnf & A_CNF_10B_T;
		break;
	case A_CNF_MEDIA_AUI:
		result = lp->adapter_cnf & A_CNF_AUI;
		break;
	case A_CNF_MEDIA_10B_2:
		result = lp->adapter_cnf & A_CNF_10B_2;
		break;
        default:
		result = lp->adapter_cnf & 
		         (A_CNF_10B_T | A_CNF_AUI | A_CNF_10B_2);
        }
        if (!result) {
                printk("%s: EEPROM is configured for unavailable media\n", dev->name);
        release_irq:
                writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) & ~(SERIAL_TX_ON | SERIAL_RX_ON));
                irq2dev_map[dev->irq] = 0;
		return -EAGAIN;
	}

        /* set the hardware to the configured choice */
	switch(lp->adapter_cnf & A_CNF_MEDIA_TYPE) {
	case A_CNF_MEDIA_10B_T:
                result = detect_tp(dev);
                if (!result) printk("%s: 10Base-T (RJ-45) has no cable\n", dev->name);
                if (lp->auto_neg_cnf & IMM_BIT) /* check "ignore missing media" bit */
                        result = A_CNF_MEDIA_10B_T; /* Yes! I don't care if I see a link pulse */
		break;
	case A_CNF_MEDIA_AUI:
	  printk("AUI?  What stinking AUI?\n");
		break;
	case A_CNF_MEDIA_10B_2:
	  printk("10Base2?  What stinking 10Base2?\n");
		break;
	case A_CNF_MEDIA_AUTO:
		writereg(dev, PP_LineCTL, lp->linectl | AUTO_AUI_10BASET);
		if (lp->adapter_cnf & A_CNF_10B_T)
			if ((result = detect_tp(dev)) != 0)
				break;

		printk("%s: no media detected\n", dev->name);
                goto release_irq;
	}
	switch(result) {
	case 0: printk("%s: no network cable attached to configured media\n", dev->name);
                goto release_irq;
	case A_CNF_MEDIA_10B_T: printk("%s: using 10Base-T (RJ-45)\n", dev->name);break;
	case A_CNF_MEDIA_AUI:   printk("%s: using 10Base-5 (AUI)\n", dev->name);break;
	case A_CNF_MEDIA_10B_2: printk("%s: using 10Base-2 (BNC)\n", dev->name);break;
	default: printk("%s: unexpected result was %x\n", dev->name, result); goto release_irq;
	}

	/* Turn on both receive and transmit operations */
	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) | SERIAL_RX_ON | SERIAL_TX_ON);

	/* Receive only error free packets addressed to this card */
	lp->rx_mode = 0;
	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT);

	lp->curr_rx_cfg = RX_OK_ENBL | RX_CRC_ERROR_ENBL;
	if (lp->isa_config & STREAM_TRANSFER)
		lp->curr_rx_cfg |= RX_STREAM_ENBL;

	writereg(dev, PP_RxCFG, lp->curr_rx_cfg);

	writereg(dev, PP_TxCFG, TX_LOST_CRS_ENBL | TX_SQE_ERROR_ENBL | TX_OK_ENBL |
	       TX_LATE_COL_ENBL | TX_JBR_ENBL | TX_ANY_COL_ENBL | TX_16_COL_ENBL);

	writereg(dev, PP_BufCFG, READY_FOR_TX_ENBL | RX_MISS_COUNT_OVRFLOW_ENBL |
		 TX_COL_COUNT_OVRFLOW_ENBL | TX_UNDERRUN_ENBL);

	/* now that we've got our act together, enable everything */
	writereg(dev, PP_BusCTL, ENABLE_IRQ
                 );
	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;
	return 0;
}

static int
net_send_packet(struct sk_buff *skb, struct device *dev)
{
	if (dev->tbusy) {
		/* If we get here, some higher level has decided we are broken.
		   There should really be a "kick me" function call instead. */
		int tickssofar = jiffies - dev->trans_start;
		if (tickssofar < 5)
			return 1;
		if (net_debug > 0) printk("%s: transmit timed out, %s?\n", dev->name,
			   tx_done(dev) ? "IRQ conflict" : "network cable problem");
		/* Try to restart the adaptor. */
		dev->tbusy=0;
		dev->trans_start = jiffies;
	}

	/* If some higher layer thinks we've missed an tx-done interrupt
	   we are passed NULL. Caution: dev_tint() handles the cli()/sti()
	   itself. */
	if (skb == NULL) {
		dev_tint(dev);
		return 0;
	}

	/* Block a timer-based transmit from overlapping.  This could better be
	   done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */
	if (set_bit(0, (void*)&dev->tbusy) != 0)
		printk("%s: Transmitter access conflict.\n", dev->name);
	else {
		struct net_local *lp = (struct net_local *)dev->priv;
		unsigned long ioaddr = dev->base_addr;
		unsigned long flags;

		if (net_debug > 3)printk("%s: sent %ld byte packet of type %x\n", dev->name, skb->len, (skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);

		/* keep the upload from being interrupted, since we
                   ask the chip to start transmitting before the
                   whole packet has been completely uploaded. */
		save_flags(flags);
		cli();

		/* initiate a transmit sequence */
		outw(lp->send_cmd, ioaddr + TX_CMD_PORT);
		outw(skb->len, ioaddr + TX_LEN_PORT);

		/* Test to see if the chip has allocated memory for the packet */
		if ((readreg(dev, PP_BusST) & READY_FOR_TX_NOW) == 0) {
			/* Gasp!  It hasn't.  But that shouldn't happen since
			   we're waiting for TxOk, so return 1 and requeue this packet. */
			restore_flags(flags);
			printk("cs8900 did not allocate memory for tx!\n");
			return 1;
		}

		/* Write the contents of the packet */
                outsw(ioaddr + TX_FRAME_PORT,skb->data,(skb->len+1) >>1);

		restore_flags(flags);
		dev->trans_start = jiffies;
	}
	dev_kfree_skb (skb, FREE_WRITE);

	return 0;
}


/* The typical workload of the driver:
   Handle the network interface interrupts. */
void
cs8900_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
        struct device *dev = (struct device *)(irq2dev_map[/* FIXME */0]);
	struct net_local *lp;
	int ioaddr, status;

	dev = irq2dev_map[0];
	if (dev == NULL) {
		printk ("net_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}
	if (dev->interrupt)
		printk("%s: Re-entering the interrupt handler.\n", dev->name);
	dev->interrupt = 1;

	ioaddr = dev->base_addr;
	lp = (struct net_local *)dev->priv;

	/* we MUST read all the events out of the ISQ, otherwise we'll never
           get interrupted again.  As a consequence, we can't have any limit
           on the number of times we loop in the interrupt handler.  The
           hardware guarantees that eventually we'll run out of events.  Of
           course, if you're on a slow machine, and packets are arriving
           faster than you can read them off, you're screwed.  Hasta la
           vista, baby!  */
	while ((status = readword(dev, ISQ_PORT))) {
		if (net_debug > 4)printk("%s: event=%04x\n", dev->name, status);
		switch(status & ISQ_EVENT_MASK) {
		case ISQ_RECEIVER_EVENT:
			/* Got a packet(s). */
			net_rx(dev);
			break;
		case ISQ_TRANSMITTER_EVENT:
			lp->stats.tx_packets++;
			dev->tbusy = 0;
			mark_bh(NET_BH);	/* Inform upper layers. */
			if ((status & TX_OK) == 0) lp->stats.tx_errors++;
			if (status & TX_LOST_CRS) lp->stats.tx_carrier_errors++;
			if (status & TX_SQE_ERROR) lp->stats.tx_heartbeat_errors++;
			if (status & TX_LATE_COL) lp->stats.tx_window_errors++;
			if (status & TX_16_COL) lp->stats.tx_aborted_errors++;
			break;
		case ISQ_BUFFER_EVENT:
			if (status & READY_FOR_TX) {
				/* we tried to transmit a packet earlier,
                                   but inexplicably ran out of buffers.
                                   That shouldn't happen since we only ever
                                   load one packet.  Shrug.  Do the right
                                   thing anyway. */
				dev->tbusy = 0;
				mark_bh(NET_BH);	/* Inform upper layers. */
			}
			if (status & TX_UNDERRUN) {
				if (net_debug > 0) printk("%s: transmit underrun\n", dev->name);
                                lp->send_underrun++;
                                if (lp->send_underrun > 3) lp->send_cmd = TX_AFTER_ALL;
                        }
			break;
		case ISQ_RX_MISS_EVENT:
			lp->stats.rx_missed_errors += (status >>6);
			break;
		case ISQ_TX_COL_EVENT:
			lp->stats.collisions += (status >>6);
			break;
		}
	}
	dev->interrupt = 0;
	return;
}

/* We have a good packet(s), get it/them out of the buffers. */
static void
net_rx(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
	struct sk_buff *skb;
	int status, length;

	status = inw(ioaddr + RX_FRAME_PORT);
	length = inw(ioaddr + RX_FRAME_PORT);
	if ((status & RX_OK) == 0) {
		lp->stats.rx_errors++;
		if (status & RX_RUNT) lp->stats.rx_length_errors++;
		if (status & RX_EXTRA_DATA) lp->stats.rx_length_errors++;
		if (status & RX_CRC_ERROR) if (!(status & (RX_EXTRA_DATA|RX_RUNT)))
			/* per str 172 */
			lp->stats.rx_crc_errors++;
		if (status & RX_DRIBBLE) lp->stats.rx_frame_errors++;
		return;
	}

	/* Malloc up new buffer. */
	skb = alloc_skb(length, GFP_ATOMIC);
	if (skb == NULL) {
		printk("%s: Memory squeeze, dropping packet.\n", dev->name);
		lp->stats.rx_dropped++;
		return;
	}
	skb->len = length;
	skb->dev = dev;

        insw(ioaddr + RX_FRAME_PORT, skb->data, length >> 1);
	if (length & 1)
		skb->data[length-1] = inw(ioaddr + RX_FRAME_PORT);

	if (net_debug > 3)printk("%s: received %d byte packet of type %x\n",
                                 dev->name, length,
                                 (skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
        skb->protocol=eth_type_trans(skb,dev);

	netif_rx(skb);
	lp->stats.rx_packets++;
	return;
}

/* The inverse routine to net_open(). */
static int
net_close(struct device *dev)
{
	/* FIXME:  This needs to put the chip to sleep and turn off the irq */
	writereg(dev, PP_RxCFG, 0);
	writereg(dev, PP_TxCFG, 0);
	writereg(dev, PP_BufCFG, 0);
	writereg(dev, PP_BusCTL, 0);

	dev->start = 0;

#if 0
	free_irq(dev->irq, NULL);
#endif

	irq2dev_map[/* FIXME */ 0] = 0;

	/* Update the statistics here. */

	return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */
static struct enet_statistics *
net_get_stats(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	cli();
	/* Update the statistics from the device registers. */
	lp->stats.rx_missed_errors += (readreg(dev, PP_RxMiss) >> 6);
	lp->stats.collisions += (readreg(dev, PP_TxCol) >> 6);
	sti();

	return &lp->stats;
}

static void set_multicast_list(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	if(dev->flags&IFF_PROMISC)
	{
		lp->rx_mode = RX_ALL_ACCEPT;
	}
	else if((dev->flags&IFF_ALLMULTI)||dev->mc_list)
	{
		/* The multicast-accept list is initialized to accept-all, and we
		   rely on higher-level filtering for now. */
		lp->rx_mode = RX_MULTCAST_ACCEPT;
	} 
	else
		lp->rx_mode = 0;

	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT | lp->rx_mode);

	/* in promiscuous mode, we accept errored packets, so we have to enable interrupts on them also */
	writereg(dev, PP_RxCFG, lp->curr_rx_cfg |
	     (lp->rx_mode == RX_ALL_ACCEPT? (RX_CRC_ERROR_ENBL|RX_RUNT_ENBL|RX_EXTRA_DATA_ENBL) : 0));
}

static int
set_mac_address(struct device *dev, void *addr)
{
	int i;
	if (dev->start)
		return -EBUSY;
	printk("%s: Setting MAC address to ", dev->name);
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i] = ((unsigned char *)addr)[i]);
	printk(".\n");
	/* set the Ethernet address */
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	return 0;
}

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/include -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -DMODULE -DCONFIG_MODVERSIONS -c cs89x0.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
