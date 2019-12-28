/* 
 *    PlayStation timers
 */
 
#ifndef __ASM_PS_TIMER_H 
#define __ASM_PS_TIMER_H 

/*
 * Timer registers
 */
#define TIMER0_COUNT_REG   0x1f801100  /* counter register */
#define TIMER0_MODE_REG    0x1f801104  /* mode register */
#define TIMER0_TARGET_REG  0x1f801108  /* target register */
#define TIMER1_COUNT_REG   0x1f801110  /* counter register */
#define TIMER1_MODE_REG    0x1f801114  /* mode register */
#define TIMER1_TARGET_REG  0x1f801118  /* target register */
#define TIMER2_COUNT_REG   0x1f801120  /* counter register */
#define TIMER2_MODE_REG    0x1f801124  /* mode register */
#define TIMER2_TARGET_REG  0x1f801128  /* target register */

/*
 * Timer mode register bits
 */
#define TIMER_STOP   	0x001    /* stop timer */
#define TIMER_TO_TARGET 0x008    /* count to value in target register */
#define TIMER_INT    	0x050    /* enable timer interrupt */
#define TIMER_CLOCK  	0x100    /* timer clock type */
#define TIMER_DIVSC  	0x200    /* timer clock type */

#endif 
