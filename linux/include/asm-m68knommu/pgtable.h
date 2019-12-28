#ifndef _M68KNOMMU_PGTABLE_H
#define _M68KNOMMU_PGTABLE_H

#include <linux/config.h>
#include <asm/setup.h>

#define page_address(page)   ({ if (!(page)->virtual) BUG(); (page)->virtual;})
#define __page_address(page) (PAGE_OFFSET + (((page) - mem_map) << PAGE_SHIFT))

#define PAGE_NONE		__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_SHARED		__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_COPY		__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_READONLY	__pgprot(0)    /* these mean nothing to NO_MM */
#define PAGE_KERNEL		__pgprot(0)    /* these mean nothing to NO_MM */

extern void paging_init(void);

#endif /* _M68KNOMMU_PGTABLE_H */
