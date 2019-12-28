/*
 * init.c: PROM library initialisation code.
 */
#include <linux/init.h>
#include <linux/config.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>

extern void prom_meminit(unsigned int);
extern void prom_identify_arch(unsigned int);
extern void prom_init_cmdline(int, char **, unsigned long);

/*
 * Set the callback vectors.
 */
void __init which_prom(unsigned long magic, int *prom_vec)
{
} 

int __init prom_init(int argc, char **argv,
	       unsigned long magic, int *prom_vec)
{
	which_prom(magic, prom_vec);
	prom_meminit(magic);
	prom_identify_arch(magic);
	prom_init_cmdline(argc, argv, magic);

	return 0;
}

