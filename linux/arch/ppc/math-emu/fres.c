/* $Id: fres.c,v 1.1.1.1 2001/02/22 14:58:10 serg Exp $
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

int
fres(void *frD, void *frB)
{
#ifdef DEBUG
	printk("%s: %p %p\n", __FUNCTION__, frD, frB);
#endif
	return -ENOSYS;
}
