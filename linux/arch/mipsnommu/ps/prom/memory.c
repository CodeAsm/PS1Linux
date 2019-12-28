/*
 * memory.c: memory initialisation code.
 */
#include <linux/init.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>

#include <asm/addrspace.h>
#include <asm/page.h>

#include <asm/bootinfo.h>

static void __init setup_memory_region(void)
{
	add_memory_region(0, 2 << 20, BOOT_MEM_RAM);
}

void __init prom_meminit(unsigned int magic)
{
		setup_memory_region();
}

void prom_free_prom_memory (void)
{
	unsigned long addr, end;
	extern	char _ftext;

	/*
	 * Free everything below the kernel itself but leave
	 * the first page reserved for the exception handlers.
	 */

	end = PHYSADDR(&_ftext);

	addr = PAGE_SIZE;
	while (addr < end) {
		ClearPageReserved(virt_to_page(addr));
		set_page_count(virt_to_page(addr), 1);
		free_page(addr);
		addr += PAGE_SIZE;
	}

	printk("Freeing unused PROM memory: %ldk freed\n",
	       (end - PAGE_SIZE) >> 10);
}
