#ifndef __MIPSNOMMU_VIRT_CONVERT__
#define __MIPSNOMMU_VIRT_CONVERT__

#ifdef __KERNEL__

#include <asm/addrspace.h>

/*
 * Change virtual addresses to physical addresses and vv.
 * These are trivial on the 1:1 Linux/MIPS mapping
 */
extern inline unsigned long virt_to_phys(volatile void * address)
{
	return PHYSADDR(address);
}

extern inline void * phys_to_virt(unsigned long address)
{
	return (void *)KSEG0ADDR(address);
}

/*
 * IO bus memory addresses are also 1:1 with the physical address
 */
extern inline unsigned long virt_to_bus(volatile void * address)
{
	return PHYSADDR(address);
}

extern inline void * bus_to_virt(unsigned long address)
{
	return (void *)KSEG0ADDR(address);
}

#endif
#endif
