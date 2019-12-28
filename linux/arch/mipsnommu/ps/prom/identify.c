/*
 * identify.c: machine identification code.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <asm/bootinfo.h>

extern unsigned long mips_machgroup;
extern unsigned long mips_machtype;

void __init prom_identify_arch (unsigned int magic)
{
	mips_machgroup = MACH_GROUP_PS;
	mips_machtype = MACH_PSX;
}


