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
	unsigned long mem_start, mem_size;
	volatile int * mem_size_reg = (int *)0x1f801060;

	*mem_size_reg = 0x888;	// !!! we have 2 Mb memory
	
	mem_start = 0;
	mem_size = 2 << 20;
	add_memory_region(mem_start, mem_size, BOOT_MEM_RAM);
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
	       (end - (PAGE_SIZE)) >> 10);
}

int is_in_rom(unsigned long addr) {

// !!! this is stub now - fix me !!!

		return 0;
}
