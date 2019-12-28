/*
 *    PlayStation SIO routines
 */

#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/ps/sio.h>

	.text
	.set	noreorder
	.align	5
   
/* 
 * Get SIO status register value
 * Ret: a3, v0 - SIO status register value
 */
LEAF(ps_sio_get_status)
		lw	v0, SIO_DATA_REG
		nop
		lhu	v0, 4(v0)
		nop
		andi	a3, v0, 0xFFFF
		jr	ra
		move	v0, a3
	END(ps_sio_get_status)

/* 
 * Get SIO control register value
 * Ret: a3, v0 - SIO control register value
 */
LEAF(ps_sio_get_control)
		lw	v0, SIO_DATA_REG
		nop
		lhu	v0, 0xA(v0)
		nop
		andi	a3, v0, 0xFFFF
		jr	ra
		move	v0, a3
	END(ps_sio_get_control)
   
/* 
 * Get SIO mode register value
 * Ret: a3, v0 - SIO mode register value
 */
LEAF(ps_sio_get_mode)
		lw	v0, SIO_DATA_REG
		nop
		lhu	v0, 8(v0)
		nop
		andi	a3, v0, 0xFFFF
		jr	ra
		move	v0, a3
	END(ps_sio_get_mode)
   
/* 
 * Get SIO baundrate register value
 * Ret: a3, v0 - SIO baundrate register value
 * !!! cause break 7(1c00), 6(1800) !!!
 */
LEAF(ps_sio_get_baundrate)
		lw	v1, SIO_DATA_REG
		lui	v0, 0x1F
		lhu	v1, 0xE(v1)	 /* v1 -	SIO baudrate factor */
		ori	v0, 0xA400
		div	v0, v1
		bnez	v1, 1f
		nop
		break	 7
1:
		li	t0, -1
		bne	v1, t0, 1f
		lui	t0, 0x8000
		bne	v0, t0, 1f
		nop
		break	 6
1:
		mflo	v0
		nop
		nop
		andi	a3, v0, 0xFFFF
		jr	ra
		move	v0, a3
	END(ps_sio_get_baundrate)
   
/* 
 * Get SIO data byte
 * Ret: a3, v0 - SIO data byte
 */
LEAF(ps_sio_get_byte)
		lw	v0, SIO_DATA_REG
		nop
		lbu	v0, 0(v0)
		nop
		andi	a3, v0, 0xFF
		jr	ra
		move	v0, a3
	END(ps_sio_get_byte)
   
/* 
 * Set SIO control register
 * In: a0 - new register value
 */
LEAF(ps_sio_set_control)
		lw	v1, SIO_DATA_REG
		andi	v0, a0, 0x1CFF  /* buffer size	always 1 byte */
		sh	v0, 0xA(v1)
		jr	ra
		move	v0, a3
	END(ps_sio_set_control)
   
/* 
 * Set SIO mode register
 * In: a0 - new register value
 */
LEAF(ps_sio_set_mode)
		lw	v1, SIO_DATA_REG
		andi	v0, a0, 0xFE
		ori	v0, 2
		sh	v0, 8(v1)
		jr	ra
		move	v0, a3
	END(ps_sio_set_mode)
   
/* 
 * Set SIO baundrate register
 * In: a0 - new baundrate value
 * !!! cause break 7(1c00) !!!
 */
LEAF(ps_sio_set_baundrate)
		li	v0, 2073600
		divu	v0, a0	 /* base	frequency */
		lw	v1, SIO_DATA_REG
		mflo	v0
		sh	v0, 0xE(v1)
		bnez	a0, 1f
		nop
		break	 7
1:
		jr	ra
		move	v0, a3
	END(ps_sio_set_baundrate)
   
/* 
 * Set SIO data byte
 * In: a0 - data byte
 */
LEAF(ps_sio_set_byte)
		lw	v0, SIO_DATA_REG
		sb	a0, 0(v0)
		jr	ra
		move	v0, a3
	END(ps_sio_set_byte)
