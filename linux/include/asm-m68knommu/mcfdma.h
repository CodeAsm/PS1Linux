/****************************************************************************/

/*
 *	mcfdma.h -- Coldfire internal DMA support defines.
 *
 *	(C) Copyright 1999, Rob Scott (rscott@mtrob.ml.org)
 */

/****************************************************************************/
#ifndef	mcfdma_h
#define	mcfdma_h
/****************************************************************************/

#include <linux/config.h>

/*
 *	Get address specific defines for this Coldfire member.
 */
#if defined(CONFIG_M5206) || defined(CONFIG_M5206e)
#define	MCFDMA_BASE0		0x200		/* Base address of DMA 0 */
#define	MCFDMA_BASE1		0x240		/* Base address of DMA 1 */
#elif defined(CONFIG_M5307) || defined(CONFIG_M5407)
#define	MCFDMA_BASE0		0x300		/* Base address of DMA 0 */
#define	MCFDMA_BASE1		0x340		/* Base address of DMA 1 */
#define	MCFDMA_BASE2		0x380		/* Base address of DMA 2 */
#define	MCFDMA_BASE3		0x3C0		/* Base address of DMA 3 */
#endif


/*
 *	Define the DMA register set addresses.
 *      Note: these are longword registers, use unsigned long as data type
 */
#define	MCFDMA_SAR		0x00		/* DMA source address (r/w) */
#define	MCFDMA_DAR		0x01		/* DMA destination adr (r/w) */
/* these are word registers, use unsigned short data type */
#define	MCFDMA_DCR		0x04		/* DMA control reg (r/w) */
#define	MCFDMA_BCR		0x06		/* DMA byte count reg (r/w) */
/* these are byte registers, use unsiged char data type */
#define	MCFDMA_DSR		0x10		/* DMA status reg (r/w) */
#define	MCFDMA_DIVR		0x14		/* DMA interrupt vec (r/w) */



/*
 *	Bit definitions for the DMA Control Register (DCR).
 */
#define	MCFDMA_DCR_INT	        0x8000		/* Enable completion irq */
#define	MCFDMA_DCR_EEXT	        0x4000		/* Enable external DMA req */
#define	MCFDMA_DCR_CS 	        0x2000		/* Enable cycle steal */
#define	MCFDMA_DCR_AA   	0x1000		/* Enable auto alignment */
#define	MCFDMA_DCR_BWC_MASK  	0x0E00		/* Bandwidth ctl mask */
#define MCFDMA_DCR_BWC_512      0x0200          /* Bandwidth:   512 Bytes */
#define MCFDMA_DCR_BWC_1024     0x0400          /* Bandwidth:  1024 Bytes */
#define MCFDMA_DCR_BWC_2048     0x0600          /* Bandwidth:  2048 Bytes */
#define MCFDMA_DCR_BWC_4096     0x0800          /* Bandwidth:  4096 Bytes */
#define MCFDMA_DCR_BWC_8192     0x0a00          /* Bandwidth:  8192 Bytes */
#define MCFDMA_DCR_BWC_16384    0x0c00          /* Bandwidth: 16384 Bytes */
#define MCFDMA_DCR_BWC_32768    0x0e00          /* Bandwidth: 32768 Bytes */
#define	MCFDMA_DCR_SAA         	0x0100		/* Single Address Access */
#define	MCFDMA_DCR_S_RW        	0x0080		/* SAA read/write value */
#define	MCFDMA_DCR_SINC        	0x0040		/* Source addr inc enable */
#define	MCFDMA_DCR_SSIZE_MASK  	0x0030		/* Src xfer size */
#define	MCFDMA_DCR_SSIZE_LONG  	0x0000		/* Src xfer size, 00 = longw */
#define	MCFDMA_DCR_SSIZE_BYTE  	0x0010		/* Src xfer size, 01 = byte */
#define	MCFDMA_DCR_SSIZE_WORD  	0x0020		/* Src xfer size, 10 = word */
#define	MCFDMA_DCR_SSIZE_LINE  	0x0030		/* Src xfer size, 11 = line */
#define	MCFDMA_DCR_DINC        	0x0008		/* Dest addr inc enable */
#define	MCFDMA_DCR_DSIZE_MASK  	0x0006		/* Dest xfer size */
#define	MCFDMA_DCR_DSIZE_LONG  	0x0000		/* Dest xfer size, 00 = long */
#define	MCFDMA_DCR_DSIZE_BYTE  	0x0002		/* Dest xfer size, 01 = byte */
#define	MCFDMA_DCR_DSIZE_WORD  	0x0004		/* Dest xfer size, 10 = word */
#define	MCFDMA_DCR_DSIZE_LINE  	0x0006		/* Dest xfer size, 11 = line */
#define	MCFDMA_DCR_START       	0x0001		/* Start transfer */

/*
 *	Bit definitions for the DMA Status Register (DSR).
 */
#define	MCFDMA_DSR_CE	        0x40		/* Config error */
#define	MCFDMA_DSR_BES	        0x20		/* Bus Error on source */
#define	MCFDMA_DSR_BED 	        0x10		/* Bus Error on dest */
#define	MCFDMA_DSR_REQ   	0x04		/* Requests remaining */
#define	MCFDMA_DSR_BSY  	0x02		/* Busy */
#define	MCFDMA_DSR_DONE        	0x01		/* DMA transfer complete */

/****************************************************************************/
#endif	/* mcfdma_h */



