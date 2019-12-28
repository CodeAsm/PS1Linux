/*
 * cmdline.c: read the command line passed to us by the PROM.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <asm/bootinfo.h>

char arcs_cmdline[COMMAND_LINE_SIZE];

void __init prom_init_cmdline(int argc, char **argv, unsigned long magic)
{
   arcs_cmdline[0] = 0;
   strcpy (arcs_cmdline, "root=/dev/bu0 console=ttyS0");
}

