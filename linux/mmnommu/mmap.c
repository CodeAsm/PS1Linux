/*
 *	linux/mm/mmap.c
 *
 *  Copyright (c) 2001 Lineo, Inc. David McCullough <davidm@lineo.com>
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 *  Written by obz.
 */
#include <linux/slab.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/file.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

kmem_cache_t *vm_area_cachep;

asmlinkage unsigned long sys_brk(unsigned long brk)
{
	return -ENOSYS;
}

/* Combine the mmap "prot" and "flags" argument into one "vm_flags" used
 * internally. Essentially, translate the "PROT_xxx" and "MAP_xxx" bits
 * into "VM_xxx".
 */
static inline unsigned long vm_flags(unsigned long prot, unsigned long flags)
{
#define _trans(x,bit1,bit2) \
((bit1==bit2)?(x&bit1):(x&bit1)?bit2:0)

	unsigned long prot_bits, flag_bits;
	prot_bits =
		_trans(prot, PROT_READ, VM_READ) |
		_trans(prot, PROT_WRITE, VM_WRITE) |
		_trans(prot, PROT_EXEC, VM_EXEC);
	flag_bits =
		_trans(flags, MAP_GROWSDOWN, VM_GROWSDOWN) |
		_trans(flags, MAP_DENYWRITE, VM_DENYWRITE) |
		_trans(flags, MAP_EXECUTABLE, VM_EXECUTABLE);
	return prot_bits | flag_bits;
#undef _trans
}

#ifdef DEBUG
static void show_process_blocks(void)
{
	struct mm_tblock_struct * tblock, *tmp;
	
	printk("Process blocks %d:", current->pid);
	
	tmp = &current->mm->tblock;
	while (tmp) {
		printk(" %p: %p", tmp, tmp->rblock);
		if (tmp->rblock)
			printk(" (%d @%p #%d)", ksize(tmp->rblock->kblock), tmp->rblock->kblock, tmp->rblock->refcount);
		if (tmp->next)
			printk(" ->");
		else
			printk(".");
		tmp = tmp->next;
	}
	printk("\n");
}
#endif /* DEBUG */

extern unsigned long askedalloc, realalloc;

unsigned long do_mmap_pgoff(
	struct file * file,
	unsigned long addr,
	unsigned long len,
	unsigned long prot,
	unsigned long flags,
	unsigned long pgoff)
{
	void * result;
	struct mm_tblock_struct * tblock;

	if ((flags & MAP_SHARED) && (prot & PROT_WRITE) && (file)) {
		printk("MAP_SHARED not supported (cannot write mappings to disk)\n");
		return -EINVAL;
	}
	
	if ((prot & PROT_WRITE) && (flags & MAP_PRIVATE)) {
		printk("Private writable mappings not supported\n");
		return -EINVAL;
	}
	
	/*
	 * determine the object being mapped and call the appropriate
	 * specific mapper. 
	 */

	if (file) {
		struct vm_area_struct vma;
		int error;
		
		
		if (!file->f_op)
			return -ENODEV;

		vma.vm_start = addr;
		vma.vm_end = addr + len;
		vma.vm_flags = vm_flags(prot,flags) /*| mm->def_flags*/;

		if (file->f_mode & 1)
			vma.vm_flags |= VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC;
		if (flags & MAP_SHARED) {
			vma.vm_flags |= VM_SHARED | VM_MAYSHARE;
			/*
			 * This looks strange, but when we don't have the file open
			 * for writing, we can demote the shared mapping to a simpler
			 * private mapping. That also takes care of a security hole
			 * with ptrace() writing to a shared mapping without write
			 * permissions.
			 *
			 * We leave the VM_MAYSHARE bit on, just to get correct output
			 * from /proc/xxx/maps..
			 */
			if (!(file->f_mode & 2))
				vma.vm_flags &= ~(VM_MAYWRITE | VM_SHARED);
		}
		vma.vm_offset = pgoff << PAGE_SHIFT;

		/* Then try full mmap routine, which might return a RAM pointer,
		   or do something truly complicated. */
		   
		if (file->f_op->mmap) {
			error = file->f_op->mmap(file, &vma);
				   
			/*printk("mmap mmap returned %d /%x\n", error, vma.vm_start);*/
			if (!error)
				return vma.vm_start;
			else if (error != -ENOSYS)
				return error;
		} else
			return -ENODEV; /* No mapping operations defined */

		/* An ENOSYS error indicates that mmap isn't possible (as opposed to
		   tried but failed) so we'll fall through to the copy. */
	}

	tblock = (struct mm_tblock_struct *)
                        kmalloc(sizeof(struct mm_tblock_struct), GFP_KERNEL);
	if (!tblock) {
		printk("Allocation of tblock for %lu byte allocation from process %d failed\n", len, current->pid);
		show_buffers();
		show_free_areas();
		return -ENOMEM;
	}

	tblock->rblock = (struct mm_rblock_struct *)
			kmalloc(sizeof(struct mm_rblock_struct), GFP_KERNEL);

	if (!tblock->rblock) {
		printk("Allocation of rblock for %lu byte allocation from process %d failed\n", len, current->pid);
		show_buffers();
		show_free_areas();
		kfree(tblock);
		return -ENOMEM;
	}

	
	result = kmalloc(len, GFP_KERNEL);
	if (!result) {
		printk("Allocation of length %lu from process %d failed\n", len, current->pid);
		show_buffers();
		show_free_areas();
		kfree(tblock->rblock);
		kfree(tblock);
		return -ENOMEM;
	}

	tblock->rblock->refcount = 1;
	tblock->rblock->kblock = result;
	tblock->rblock->size = len;
	
	realalloc += ksize(result);
	askedalloc += len;

#ifdef WARN_ON_SLACK	
	if ((len+WARN_ON_SLACK) <= ksize(result))
		printk("Allocation of %lu bytes from process %d has %lu bytes of slack\n", len, current->pid, ksize(result)-len);
#endif
	
	if (file) {
		int error;
		mm_segment_t old_fs = get_fs();
		set_fs(KERNEL_DS);
		error = file->f_op->read(file, (char *) result, len, &file->f_pos);
		set_fs(old_fs);
		if (error < 0) {
			kfree(result);
			kfree(tblock->rblock);
			kfree(tblock);
			return error;
		}
		if (error<len)
			memset(result+error, '\0', len-error);
	} else {
		memset(result, '\0', len);
	}

        
	realalloc += ksize(tblock);
	askedalloc += sizeof(struct mm_tblock_struct);

	realalloc += ksize(tblock->rblock);
	askedalloc += sizeof(struct mm_rblock_struct);

	tblock->next = current->mm->tblock.next;
	current->mm->tblock.next = tblock;

#ifdef DEBUG
	printk("do_mmap:\n");
	show_process_blocks();
#endif	  

	return (unsigned long)result;
}

int do_munmap(struct mm_struct * mm, unsigned long addr, size_t len)
{
	struct mm_tblock_struct * tblock, *tmp;

#ifdef DEBUG
        printk("do_munmap:\n");
#endif

	tmp = &mm->tblock; /* dummy head */
	while ((tblock=tmp->next) && (tblock->rblock) && (tblock->rblock->kblock != (void*)addr)) 
		tmp = tblock;
		
	if (!tblock) {
	        printk("munmap of non-mmaped memory by process %d (%s): %p\n", current->pid, current->comm, (void*)addr);
	        return -EINVAL;
	}
	if(tblock->rblock)
		if(!--tblock->rblock->refcount) {
			if (tblock->rblock->kblock) {
				realalloc -= ksize(tblock->rblock->kblock);
				askedalloc -= tblock->rblock->size;
				kfree(tblock->rblock->kblock);
			}
			
			realalloc -= ksize(tblock->rblock);
			askedalloc -= sizeof(struct mm_rblock_struct);
			kfree(tblock->rblock);
		}
	tmp->next = tblock->next;
	realalloc -= ksize(tblock);
	askedalloc -= sizeof(struct mm_tblock_struct);
	kfree(tblock);

#ifdef DEBUG
	show_process_blocks();
#endif	  

	return -EINVAL;
}

/* Release all mmaps. */
void exit_mmap(struct mm_struct * mm)
{
	struct mm_tblock_struct *tmp;
	/*unsigned long flags;*/

	if (!mm)
		return;

	if (mm->executable)
		iput(mm->executable);
	mm->executable = NULL;

#if DAVIDM
/*
 *	this isn't right and doesn't appear to be needed anymore, mm_count
 *	is 2 when we enter here, mm_users is 0,  schedule finally calls
 *	__mmdrop to free this but we haven't returned the memory, remove all
 *	this after it has done some miles.
 */
	/*save_flags(flags); cli();*/

	if (atomic_read(&mm->mm_count) > 1) {
		/*restore_flags(flags);*/
		return;
	}
#endif

#ifdef DEBUG
	printk("Exit_mmap:\n");
#endif

	while((tmp = mm->tblock.next)) {
		if (tmp->rblock) {
			if (!--tmp->rblock->refcount) {
				if (tmp->rblock->kblock) {
					realalloc -= ksize(tmp->rblock->kblock);
					askedalloc -= tmp->rblock->size;
					kfree(tmp->rblock->kblock);
				}
				realalloc -= ksize(tmp->rblock);
				askedalloc -= sizeof(struct mm_rblock_struct);
				kfree(tmp->rblock);
			}
			tmp->rblock = 0;
		}
		mm->tblock.next = tmp->next;
		realalloc -= ksize(tmp);
		askedalloc -= sizeof(struct mm_tblock_struct);
		kfree(tmp);
	}

#ifdef DEBUG
	show_process_blocks();
#endif	  

	/*restore_flags(flags);*/
}

asmlinkage long sys_munmap(unsigned long addr, size_t len)
{
	int ret;
	struct mm_struct *mm = current->mm;

	down(&mm->mmap_sem);
	ret = do_munmap(mm, addr, len);
	up(&mm->mmap_sem);
	return ret;
}

void __init vma_init(void)
{
	vm_area_cachep = kmem_cache_create("vm_area_struct",
					   sizeof(struct vm_area_struct),
					   0, SLAB_HWCACHE_ALIGN,
					   NULL, NULL);
	if(!vm_area_cachep)
		panic("vma_init: Cannot alloc vm_area_struct cache.");

	mm_cachep = kmem_cache_create("mm_struct",
				      sizeof(struct mm_struct),
				      0, SLAB_HWCACHE_ALIGN,
				      NULL, NULL);
	if(!mm_cachep)
		panic("vma_init: Cannot alloc mm_struct cache.");
}
