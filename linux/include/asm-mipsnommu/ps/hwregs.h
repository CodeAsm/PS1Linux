/*
 * PlayStation hardware registers ports
 */

#ifndef __ASM_PS_HWREGS_H 
#define __ASM_PS_HWREGS_H 

/*
 * Original memory parameters
 */
#define MEM_SIZE_PORT	0x1060

/*
 * Interrupt register ports
 */
#define INT_ACKN_PORT  0x1070   /* interrupt acknowledge register */
#define INT_MASK_PORT  0x1074   /* interrupt mask register */

/*
 * Timer register ports
 */
#define TIMER0_COUNT_PORT  0x1100   /* timer0 counter register */
#define TIMER0_MODE_PORT   0x1104   /* timer0 mode register */
#define TIMER0_TARGET_PORT 0x1108   /* timer0 target register */

#define TIMER1_COUNT_PORT  0x1110   /* timer1 counter register */
#define TIMER1_MODE_PORT   0x1114   /* timer1 mode register */
#define TIMER1_TARGET_PORT 0x1118   /* timer1 target register */

#define TIMER2_COUNT_PORT  0x1120   /* timer2 counter register */
#define TIMER2_MODE_PORT   0x1124   /* timer2 mode register */
#define TIMER2_TARGET_PORT 0x1128   /* timer2 target register */

/*
 * SIO register ports
 */
#define SIO_DATA_PORT 0x1050  /* sio data register */
#define SIO_STAT_PORT 0x1054  /* sio status register */
#define SIO_MODE_PORT 0x1058  /* sio mode register */
#define SIO_CTRL_PORT 0x105a  /* sio control register */
#define SIO_RATE_PORT 0x105e  /* sio baudrate register */

/*
 * GPU register ports
 */
#define GPU_DATA_PORT      0x1810  /* gpu data port */
#define GPU_CONTROL_PORT   0x1814  /* gpu control/status port */
#define GPU_STATUS_PORT    0x1814  /* gpu control/status port */

/*
 * DMA register ports
 */
/* channel 0 - MDECin */
#define DMA_MADR0_PORT  0x1080 /* dma channel 0 memory address register */
#define DMA_BCR0_PORT   0x1084 /* dma channel 0 block control register */
#define DMA_CHCR0_PORT  0x1088 /* dma channel 0 channel control register */

/* channel 1 - MDECout */
#define DMA_MADR1_PORT  0x1090 /* dma channel 1 memory address register */
#define DMA_BCR1_PORT   0x1094 /* dma channel 1 block control register */
#define DMA_CHCR1_PORT  0x1098 /* dma channel 1 channel control register */

/* channel 2 - GPU */
#define DMA_MADR2_PORT  0x10a0 /* dma channel 2 memory address register */
#define DMA_BCR2_PORT   0x10a4 /* dma channel 2 block control register */
#define DMA_CHCR2_PORT  0x10a8 /* dma channel 2 channel control register */

/* channel 3 - CD-ROM */
#define DMA_MADR3_PORT  0x10b0 /* dma channel 3 memory address register */
#define DMA_BCR3_PORT   0x10b4 /* dma channel 3 block control register */
#define DMA_CHCR3_PORT  0x10b8 /* dma channel 3 channel control register */

/* channel 4 - SPU */
#define DMA_MADR4_PORT  0x10c0 /* dma channel 4 memory address register */
#define DMA_BCR4_PORT   0x10c4 /* dma channel 4 block control register */
#define DMA_CHCR4_PORT  0x10c8 /* dma channel 4 channel control register */

/* channel 5 - PIO */
#define DMA_MADR5_PORT  0x10d0 /* dma channel 5 memory address register */
#define DMA_BCR5_PORT   0x10d4 /* dma channel 5 block control register */
#define DMA_CHCR5_PORT  0x10d8 /* dma channel 5 channel control register */

/* channel 6 - GPU OTC */
#define DMA_MADR6_PORT  0x10e0 /* dma channel 6 memory address register */
#define DMA_BCR6_PORT   0x10e4 /* dma channel 6 block control register */
#define DMA_CHCR6_PORT  0x10e8 /* dma channel 6 channel control register */

#define DMA_DPCR_PORT 0x10f0  /* dma primary control register */
#define DMA_DICR_PORT 0x10f4  /* dma interrupt control register */

/*
 * CD-ROM register ports
 */
#define CDREG0_PORT  0x1800  /* cd-rom register 0 */
#define CDREG1_PORT  0x1801  /* cd-rom register 1 */
#define CDREG2_PORT  0x1802  /* cd-rom register 2 */
#define CDREG3_PORT  0x1803  /* cd-rom register 3 */

/*
 * MDEC register ports
 */
#define MDEC_CONTROL_PORT  0x10f0  /* mdec control register */
#define MDEC_STATUS_PORT   0x10f4  /* mdec status register */

/*
 * PIO register ports
 */
#define PIO_PAGE_BASE_PORT  0x1000	/* pio page base address register port */
#define PIO_BUS_PORT		  0x1008	/* pio bus parameters register */
#define ROM_MAP_PORT		  0x1010	/* rom to pio mapping register port */

#endif
