/* 
 *    PlayStation SIO
 */
 
#ifndef __ASM_PS_SIO_H 
#define __ASM_PS_SIO_H 

/*
 * SIO registers
 */
#define SIO_DATA_REG 0x1050  /* data register */
#define SIO_STAT_REG 0x1054  /* status register */
#define SIO_MODE_REG 0x1058  /* mode register */
#define SIO_CTRL_REG 0x105a  /* control register */
#define SIO_RATE_REG 0x105e  /* baudrate register */

/*
 * SIO status register bits
 */
#define SIO_RFW   0x0001   /* ready for write byte -> TX */
#define SIO_RFR   0x0002   /* ready for read byte RX -> */
#define SIO_RFT   0x0004   /* transmitter empty */
#define SIO_PERR  0x0008   /* parity error */
#define SIO_OERR  0x0010   /* overrun error */
#define SIO_FERR  0x0020   /* frame error */
#define SIO_DSR   0x0080   /* DSR */
#define SIO_CTS   0x0100   /* CTS */
#define SIO_IRQ   0x0200   /* interrupt */

/*
 * SIO mode register bits
 */
#define SIO_BRS1     0x0001   /* baundrate prescaler = 1 */
#define SIO_BRS16    0x0002   /* baundrate prescaler = 16 */
#define SIO_BRS64    0x0003   /* baundrate prescaler = 64 */
#define SIO_CHR6     0x0004   /* char length = 6 bit */
#define SIO_CHR7     0x0008   /* char length = 7 bit */
#define SIO_CHR8     0x000c   /* char length = 8 bit */
#define SIO_PARITY   0x0010   /* parity bit: 0 - off, 1 - on */
#define SIO_PAROOD   0x0020   /* parity mode bit: 0 - even, 1 - odd */
#define SIO_SB1      0x0040   /* stop bit length = 1 */
#define SIO_SB1_5    0x0080   /* stop bit length = 1,5 */
#define SIO_SB2      0x00c0   /* stop bit length = 2 */

/*
 * SIO control register bits
 */
#define SIO_TX       0x0001   /* TX enable */
#define SIO_DTR      0x0002   /* DTR, OpenDrain-Output */
#define SIO_RX       0x0004   /* RX enable */
#define SIO_ACKIRQ	 0x0010   /* acknowledge sio irq */
#define SIO_RTS      0x0020   /* RTS, OpenDrain-Output */
#define SIO_RESET    0x0040   /* SIO reset */
#define SIO_BUF2     0x0100   /* buffer size = 2 bytes */
#define SIO_BUF4     0x0200   /* buffer size = 4 bytes */
#define SIO_BUF8     0x0300   /* buffer size = 8 bytes */
#define SIO_TXIRQ    0x0400   /* TX interrupt enable */
#define SIO_RXIRQ    0x0800   /* RX interrupt enable */
#define SIO_DSRIRQ   0x1000   /* DSR interrupt enable */
#define SIO_ALLIRQ   (SIO_DSRIRQ|SIO_RXIRQ|SIO_TXIRQ)


// SIO baude rates
#define SIO_B11520   0x12	// 115200

#ifndef __ASSEMBLY__

/*
 * SIO function prototipes
 */
int ps_sio_get_status (void);
int ps_sio_get_control (void);
int ps_sio_get_mode (void);
int ps_sio_get_baundrate (void);
int ps_sio_get_byte (void);

void ps_sio_set_control (int value);
void ps_sio_set_mode (int value);
void ps_sio_set_baundrate (int value);
void ps_sio_set_byte (int value);

#endif // __ASSEMBLY__

#ifdef __SIO_STANDALONE_DEBUG__
int sio_console_setup(void);
int sio_console_write(const char *s, unsigned count);

#endif //__SIO_STANDALONE_DEBUG__

#endif 
