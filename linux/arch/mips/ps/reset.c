/*
 *  $Id: reset.c,v 1.1 2001/02/22 19:13:55 serg Exp $
 *
 *  Reset a PlayStation.
 *
 */

void (*back_to_prom)(void) = (void (*)(void))0xBFC00000;

void ps_machine_restart(char *command)
{
	back_to_prom();
}

void ps_machine_halt(void)
{
	back_to_prom();
}

void ps_machine_power_off(void)
{
    /* !!! ??? PlayStations don't have a software power switch ??? !!! */
	back_to_prom();
}
