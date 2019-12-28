/*
 *  linux/mmnommu/memory.c
 *
 *  Copyright (c) 2000 Lineo,Inc. David McCullough <davidm@lineo.com>
 */

#include	<linux/config.h>
#include	<linux/slab.h>
#include	<linux/interrupt.h>
#include	<linux/init.h>
#include	<linux/iobuf.h>
#include	<asm/uaccess.h>

void *high_memory;
mem_map_t * mem_map = NULL;
unsigned long max_mapnr;
unsigned long num_physpages;
unsigned long askedalloc, realalloc;

/*
 * Force in an entire range of pages from the current process's user VA,
 * and pin them in physical memory.  
 */

int map_user_kiobuf(int rw, struct kiobuf *iobuf, unsigned long va, size_t len)
{
	return(0);
}


/*
 * Unmap all of the pages referenced by a kiobuf.  We release the pages,
 * and unlock them if they were locked. 
 */

void unmap_kiobuf (struct kiobuf *iobuf) 
{
}


/*
 * Lock down all of the pages of a kiovec for IO.
 *
 * If any page is mapped twice in the kiovec, we return the error -EINVAL.
 *
 * The optional wait parameter causes the lock call to block until all
 * pages can be locked if set.  If wait==0, the lock operation is
 * aborted if any locked pages are found and -EAGAIN is returned.
 */

int lock_kiovec(int nr, struct kiobuf *iovec[], int wait)
{
	return 0;
}

/*
 * Unlock all of the pages of a kiovec after IO.
 */

int unlock_kiovec(int nr, struct kiobuf *iovec[])
{
	return 0;
}

/*
 * Handle all mappings that got truncated by a "truncate()"
 * system call.
 *
 * NOTE! We have to be ready to update the memory sharing
 * between the file and the memory map for a potential last
 * incomplete page.  Ugly, but necessary.
 */
void vmtruncate(struct inode * inode, loff_t offset)
{
	struct address_space *mapping = inode->i_mapping;
	unsigned long limit;

	if (inode->i_size < offset)
		goto do_expand;
	inode->i_size = offset;
	truncate_inode_pages(mapping, offset);
	if (inode->i_op && inode->i_op->truncate)
		inode->i_op->truncate(inode);
	return;

do_expand:
	limit = current->rlim[RLIMIT_FSIZE].rlim_cur;
	if (limit != RLIM_INFINITY) {
		if (inode->i_size >= limit) {
			send_sig(SIGXFSZ, current, 0);
			goto out;
		}
		if (offset > limit) {
			send_sig(SIGXFSZ, current, 0);
			offset = limit;
		}
	}
	inode->i_size = offset;
	if (inode->i_op && inode->i_op->truncate)
		inode->i_op->truncate(inode);
out:
	return;
}


