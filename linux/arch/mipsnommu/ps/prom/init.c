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
#ifdef CONFIG_PS_SIO_CONSOLE
extern void ps_sio_console_init(void);
#endif
#ifdef CONFIG_PS_GPU_CONSOLE
extern void ps_gpu_console_init(void);
#endif

/*
 * Set the callback vectors.
 */
void __init which_prom(unsigned long magic, int *prom_vec)
{
} 

int __init prom_init(int argc, char **argv,
	       unsigned long magic, int *prom_vec)
{
#ifdef CONFIG_PS_SIO_CONSOLE
	ps_sio_console_init();
#endif
#ifdef CONFIG_PS_GPU_CONSOLE
	ps_gpu_console_init();
#endif

	which_prom(magic, prom_vec);
	prom_meminit(magic);
	prom_identify_arch(magic);
	prom_init_cmdline(argc, argv, magic);

	return 0;
}

