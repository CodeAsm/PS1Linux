/*
 * Setup the interrupt stuff.
 */
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/mc146818rtc.h>
#include <linux/param.h>
#include <linux/console.h>
#include <asm/mipsregs.h>
#include <asm/bootinfo.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/reboot.h>
#include <asm/io.h>

#include <asm/ps/interrupts.h>
#include <asm/ps/io.h>
#include <asm/ps/hwregs.h>

extern asmlinkage void playstation_handle_int(void);
extern asmlinkage void ps_intr_unimplemented(int irq, void *dev_id, struct pt_regs *regs);

void psx_init (void);

extern struct rtc_ops ps_rtc_ops;

/*
 * Information regarding the IRQ Controller
 */

extern void ps_machine_restart(char *command);
extern void ps_machine_halt(void);
extern void ps_machine_power_off(void);

static void __init ps_irq_setup(void)
{
   volatile int stat;

   switch (mips_machtype) {
      case MACH_PSX:
	      psx_init ();
         /* Unmask all irq */
         outw (0, INT_MASK_PORT);
         /* Setup specific PSX interrupt handling bit */
         stat = read_32bit_cp0_register(CP0_STATUS);
         stat |= 0x400;
         write_32bit_cp0_register(CP0_STATUS, stat);
	      break;
   }
   set_except_vector(0, playstation_handle_int);
}

void __init playstation_setup(void)
{
    irq_setup = ps_irq_setup;

    mips_io_port_base = PSX_HW_REG_BASE;

    _machine_restart = ps_machine_restart;
    _machine_halt = ps_machine_halt;
    _machine_power_off = ps_machine_power_off;
    
#ifdef CONFIG_PSX_PIO_EXTENSION
    rtc_ops = &ps_rtc_ops;
#endif
}

void __init psx_init(void)
{
    /*
     * Setup interrupt structure
     */
    cpu_mask_tbl[VBL] = VBL_MASK;
    cpu_irq_nr[VBL] = VBL;
    cpu_mask_tbl[GPU] = GPU_MASK;
    cpu_irq_nr[GPU] = GPU;
    cpu_mask_tbl[CDROM] = CDROM_MASK;
    cpu_irq_nr[CDROM] = CDROM;
    cpu_mask_tbl[DMA] = DMA_MASK;
    cpu_irq_nr[DMA] = DMA;
    cpu_mask_tbl[TIMER0] = TIMER0_MASK;
    cpu_irq_nr[TIMER0] = TIMER0;
    cpu_mask_tbl[TIMER1] = TIMER1_MASK;
    cpu_irq_nr[TIMER1] = TIMER1;
    cpu_mask_tbl[TIMER2] = TIMER2_MASK;
    cpu_irq_nr[TIMER2] = TIMER2;
    cpu_mask_tbl[CONTROLLER] = CONTROLLER_MASK;
    cpu_irq_nr[CONTROLLER] = CONTROLLER;
    cpu_mask_tbl[SIO] = SIO_MASK;
    cpu_irq_nr[SIO] = SIO;
    cpu_mask_tbl[SPU] = SPU_MASK;
    cpu_irq_nr[SPU] = SPU;
    cpu_mask_tbl[PIO] = PIO_MASK;
    cpu_irq_nr[PIO] = PIO;
}
