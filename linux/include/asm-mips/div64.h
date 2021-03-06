/* $Id: div64.h,v 1.1.1.1 2001/02/22 14:58:57 serg Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef _ASM_DIV64_H
#define _ASM_DIV64_H

/*
 * Hey, we're already 64-bit, no
 * need to play games..
 */
#define do_div(n,base) ({ \
	int __res; \
	__res = ((unsigned long) n) % (unsigned) base; \
	n = ((unsigned long) n) / (unsigned) base; \
	__res; })

#endif /* _ASM_DIV64_H */
