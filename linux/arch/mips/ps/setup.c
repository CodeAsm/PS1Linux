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

extern asmlinkage void playstation_handle_int(void);

void psx_init (void);

/*
 * Information regarding the IRQ Controller
 */

extern void ps_machine_restart(char *command);
extern void ps_machine_halt(void);
extern void ps_machine_power_off(void);

extern void intr_halt(void);

extern int setup_ps_irq(int, struct irqaction *);

void (*board_time_init) (struct irqaction * irq);

static void __init ps_irq_setup(void)
{
   switch (mips_machtype) {
      case MACH_PSX:
	      psx_init ();
	      break;
   }
   set_except_vector(0, playstation_handle_int);
}

/*
 * enable the periodic interrupts
 */
static void __init ps_time_init(struct irqaction *irq) /* !!! */
{
    /*
     * Here we go, enable periodic rtc interrupts.
     */

    setup_ps_irq(CLOCK, irq);
}

void __init playstation_setup(void)
{
    irq_setup = ps_irq_setup;
    board_time_init = ps_time_init;

    _machine_restart = ps_machine_restart;
    _machine_halt = ps_machine_halt;
    _machine_power_off = ps_machine_power_off;
}

void __init psx_init(void)  /* !!! */
{
}
