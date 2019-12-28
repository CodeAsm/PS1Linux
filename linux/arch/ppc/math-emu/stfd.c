/* $Id: stfd.c,v 1.1.1.1 2001/02/22 14:58:10 serg Exp $
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
 
int
stfd(void *frS, void *ea)
{
#if 0
#ifdef DEBUG
	printk("%s: S %p, ea %p: ", __FUNCTION__, frS, ea);
	dump_double(frS);
	printk("\n");
#endif
#endif

	if (copy_to_user(ea, frS, sizeof(double)))
		return -EFAULT;

	return 0;
}
