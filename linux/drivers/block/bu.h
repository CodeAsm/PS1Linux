#ifndef __BU_H__
#define __BU_H__

#include "asm/types.h"

#define BU_SIZE         (0x20000)
#define BU_BLK_SIZE     (128)
#define BU_BLK_SHIFT    (7)
#define BU_MINORS       (2)

// device data
typedef struct _bu_device {
   int in_use;
   int state;
   int timeout;
} bu_device;

#define BU_NONE      0
#define BU_WAIT      1
#define BU_READY     2
#define BU_TIMEOUT   3

typedef struct _bu_request_t
{
	int	  floor;				// channel number 0..f
	__u16 block;				// block number
	int		card;					// 0 for bu0 and 1 for bu1
	__u8  buffer[BU_BLK_SIZE];	// data buffer
  int   mode;					// 'R' - read, 'W' - write
} bu_request_t;

typedef struct
{
// driver private data
  bu_request_t *bu_request;
	__u8  cs;						// checksum
	int		cnt;					// counter
	int 	state;				// irq FSM state
	int		stop;					// stop packet transfer
  int   hw_state;			// hardware state
  __u8	byte;					// last byte received from controller
} bu_t; 

#define BU_DATA		 0x1040
#define BU_STATUS	 0x1044
#define BU_CONTROL 0x104A
#define BU_REG8		 0x1048
#define BU_REGE		 0x104E
#define PIC_ACK		 0x1070
#define PIC_MASK	 0x1074


#define BU_FLOOR_SHIFT 10
#define BU_BLOCK_MASK 0x3ff
#define BU_FLOOR_MASK 0xf

#endif
