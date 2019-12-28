/*
 *  linux/fs/binfmt_flat.c
 *
 *	Copyright (C) 2000 Lineo, by David McCullough <davidm@lineo.com>
 *  based heavily on:
 *
 *  linux/fs/binfmt_aout.c:
 *      Copyright (C) 1991, 1992, 1996  Linus Torvalds
 *  linux/fs/binfmt_flat.c for 2.0 kernel
 *	    Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>
 *	JAN/99 -- coded full program relocation (gerg@lineo.com)
 */

#include <linux/module.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/a.out.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/malloc.h>
#include <linux/binfmts.h>
#include <linux/personality.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/flat.h>

#ifdef DEBUG
#define	DBG_FLT(a...)	printk(##a)
#else
#define	DBG_FLT(a...)
#endif

static int load_flat_binary(struct linux_binprm *, struct pt_regs * regs);
static int load_flat_library(struct file*);
static int flat_core_dump(long signr, struct pt_regs * regs, struct file *file);

extern void dump_thread(struct pt_regs *, struct user *);

static struct linux_binfmt flat_format = {
	NULL, THIS_MODULE, load_flat_binary, load_flat_library,
	flat_core_dump, PAGE_SIZE
};

/*
 * Routine writes a core dump image in the current directory.
 * Currently only a stub-function.
 */

static int flat_core_dump(long signr, struct pt_regs * regs, struct file *file)
{
	printk("Process %s:%d received signr %d and should have core dumped\n",
			current->comm, current->pid, (int) signr);
	return(1);
}

static unsigned long putstring(unsigned long p, char * string)
{
	unsigned long l = strlen(string)+1;
	DBG_FLT("put_string '%s'\n", string);
	p -= l;
	memcpy((void*)p, string, l);
	return p;
}

static unsigned long putstringarray(unsigned long p, int count, char ** array)
{
	DBG_FLT("putstringarray(0x%x, %d, 0x%x)\n", p, count, array);
	while (count) {
		p=putstring(p, array[--count]);
		DBG_FLT("p2=%x\n", p);
	}
	return p;
}

#if DAVIDM
static unsigned long stringarraylen(int count, char ** array)
{
	int l = 4;
	while(count) {
		l += strlen(array[--count]);
		l++;
		l+=4;
	}
	return l;
}
#endif

/*
 * create_flat_tables() parses the env- and arg-strings in new user
 * memory and creates the pointer tables from them, and puts their
 * addresses on the "stack", returning the new stack pointer value.
 */
static unsigned long create_flat_tables(
	unsigned long pp,
	struct linux_binprm * bprm)
{
	unsigned long *argv,*envp;
	unsigned long * sp;
	char * p = (char*)pp;
	int argc = bprm->argc;
	int envc = bprm->envc;
	char dummy;

	sp = (unsigned long *) ((-(unsigned long)sizeof(char *))&(unsigned long) p);

#ifdef __alpha__
/* whee.. test-programs are so much fun. */
	put_user((unsigned long) 0, --sp);
	put_user((unsigned long) 0, --sp);
	if (bprm->loader) {
		put_user((unsigned long) 0, --sp);
		put_user((unsigned long) 0x3eb, --sp);
		put_user((unsigned long) bprm->loader, --sp);
		put_user((unsigned long) 0x3ea, --sp);
	}
	put_user((unsigned long) bprm->exec, --sp);
	put_user((unsigned long) 0x3e9, --sp);
#endif

	sp -= envc+1;
	envp = sp;
	sp -= argc+1;
	argv = sp;
#if defined(__i386__) || defined(__mc68000__)
	--sp; put_user((unsigned long) envp, sp);
	--sp; put_user((unsigned long) argv, sp);
#endif
	put_user(argc,--sp);
	current->mm->arg_start = (unsigned long) p;
	while (argc-->0) {
		put_user((unsigned long) p, argv++);
		do {
			get_user(dummy, p); p++;
		} while (dummy);
	}
	put_user((unsigned long) NULL, argv);
	current->mm->arg_end = current->mm->env_start = (unsigned long) p;
	while (envc-->0) {
		put_user((unsigned long)p, envp); envp++;
		do {
			get_user(dummy, p); p++;
		} while (dummy);
	}
	put_user((unsigned long) NULL, envp);
	current->mm->env_end = (unsigned long) p;
	return (unsigned long)sp;
}



static void
do_reloc(struct flat_reloc * r)
{
	unsigned long * ptr = (unsigned long*)
		(current->mm->start_code + r->offset);


#if 0 // def DEBUG
	printk("Relocation type=%x offset=%x addr=%x [%x->",
		r->type, r->offset, ptr, *ptr);
#endif
	
	switch (r->type) {
	case FLAT_RELOC_TYPE_TEXT:
		*ptr += current->mm->start_code;
		break;
	case FLAT_RELOC_TYPE_DATA:
		*ptr += current->mm->start_data;
		break;
	case FLAT_RELOC_TYPE_BSS:
		*ptr += current->mm->end_data;
		break;
	default:
		printk("BINFMT_FLAT: Unknown relocation type=%x\n", r->type);
		break;
	}


#if 0 // def DEBUG
	printk("%x]\n", *ptr);
#endif
}



/*
 * These are the functions used to load a.out style executables and shared
 * libraries.  There is no binary dependent code anywhere else.
 */

static int load_flat_binary(struct linux_binprm * bprm, struct pt_regs * regs)
{
	struct flat_hdr * hdr;
	unsigned long error, result;
	unsigned long rlim;
	int retval;

	unsigned long pos;
	unsigned long p = bprm->p;
	unsigned long data_len, bss_len, stack_len, code_len;
	unsigned long memp = 0, memkasked = 0; /* for find brk area */
	struct inode *inode;
	loff_t fpos;

	DBG_FLT("BINFMT_FLAT: Loading file: %x\n", bprm->file);

	hdr = ((struct flat_hdr *) bprm->buf);		/* exec-header */
	inode = bprm->file->f_dentry->d_inode;

	if (strncmp(hdr->magic, "bFLT", 4) || (hdr->rev != 2)) {
		printk("bad magic/rev (%ld, need %d)\n", hdr->rev, 2);
		return -ENOEXEC;
	}

	code_len = hdr->data_start;
	data_len = hdr->data_end - hdr->data_start;
	bss_len = hdr->bss_end - hdr->data_end;
	stack_len = hdr->stack_size;
	
	/* Check initial limits. This avoids letting people circumvent
	 * size limits imposed on them by creating programs with large
	 * arrays in the data or bss.
	 */
	rlim = current->rlim[RLIMIT_DATA].rlim_cur;
	if (rlim >= RLIM_INFINITY)
		rlim = ~0;
	if (data_len + bss_len > rlim)
		return -ENOMEM;

	/* Flush all traces of the currently running executable */
	retval = flush_old_exec(bprm);
	if (retval)
		return retval;

	/* OK, This is the point of no return */
#if !defined(__sparc__)
	set_personality(PER_LINUX);
#else
	set_personality(PER_SUNOS);
#if !defined(__sparc_v9__)
	memcpy(&current->thread.core_exec, &ex, sizeof(struct exec));
#endif
#endif

	down(&current->mm->mmap_sem);
	error = do_mmap(bprm->file, 0, code_len + data_len + bss_len + stack_len,
			PROT_READ|PROT_EXEC | ((hdr->flags&FLAT_FLAG_RAM) ? PROT_WRITE : 0),
			0, 0);
	up(&current->mm->mmap_sem);
        
	if (error >= -4096) {
		printk("Unable to map flat executable, errno %d\n", (int)-error);
		return error; /* Beyond point of no return? Oh well... */
	}


	memp = error;
	memkasked = code_len + data_len + bss_len + stack_len;

	DBG_FLT("BINFMT_FLAT: mmap returned: %x\n", error);

	current->mm->executable = NULL;
	
	if (is_in_rom(error)) {
		/*
		 * do_mmap returned a ROM mapping, so allocate RAM for
		 * data + bss + stack
		 */
		unsigned long result;

		DBG_FLT("BINFMT_FLAT: ROM mapping of file\n");

		down(&current->mm->mmap_sem);
		pos = do_mmap(0, 0, data_len + bss_len + stack_len,
				PROT_READ|PROT_WRITE|PROT_EXEC, 0, 0);
		up(&current->mm->mmap_sem);
		if (pos >= (unsigned long)-4096) {
			printk("Unable to allocate RAM for process, errno %d\n", (int)-pos);
			return pos;
		}

		memp = error; /* don't use ROM mmapping for sbrk!, use tail of data */
		memkasked = data_len + bss_len + stack_len;
		
		DBG_FLT("BINFMT_FLAT: Allocated data+bss+stack (%d bytes): %x\n",
				data_len + bss_len + stack_len, pos);

		/* And then fill it in */

		fpos = hdr->data_start;
//printk("flat_read 0x%x %d %d\n", pos, data_len, fpos);
		result = bprm->file->f_op->read(bprm->file, (char *) pos, data_len, &fpos);
		if (result >= (unsigned long)-4096) {
			do_munmap(current->mm, pos, 0);
			printk("Unable to read data+bss, errno %d\n", (int)-result);
			send_sig(SIGKILL, current, 0);
			return result;
		}

		if (IS_SYNC(inode)) {
			DBG_FLT("Retaining inode\n");
			current->mm->executable = bprm->file->f_dentry->d_inode;
			atomic_inc(&inode->i_count);
		}
	} else {
		/*
		 * Since we got a RAM mapping, mmap has already allocated a block
		 * for us, and read in the data. .
		 */
		DBG_FLT("BINFMT_FLAT: RAM mapping of file\n");
		pos = error + code_len;
	}

	/* zero the BSS */
	memset((void*)(pos + data_len), 0, bss_len + stack_len);

	DBG_FLT("ROM mapping is %x, Entry point is %x, data_start is %x\n",
			error, hdr->entry, hdr->data_start);

	current->mm->start_code = error + hdr->entry;
	current->mm->end_code = error + hdr->data_start;
	current->mm->start_data = pos;
	current->mm->end_data = pos + data_len;
#ifdef NO_MM
	/*
	 *	set up the brk stuff (uses any slack left in data/bss allocation
	 */
	current->mm->brk = current->mm->start_brk = memp + ((memkasked + 3) & ~3);
	current->mm->end_brk = memp + ksize((void *) memp);
#endif

	current->mm->rss = 0;
#ifndef NO_MM
	current->mm->mmap = NULL;
#endif

	DBG_FLT("Load %s: TEXT=%x-%x DATA=%x-%x BSS=%x-%x\n",
		bprm->argv[0],
		(int) current->mm->start_code, (int) current->mm->end_code,
		(int) current->mm->start_data, (int) current->mm->end_data,
		(int) current->mm->end_data, (int) current->mm->brk);

	if (is_in_rom(error)) {
		int r;
		for(r = 0; r < hdr->reloc_count; r++) {
			struct flat_reloc * reloc = (struct flat_reloc*)
				(error + hdr->reloc_start + (sizeof(struct flat_reloc)*r));
			do_reloc(reloc);
		}
	} else {
		int r;
		for(r = 0; r < hdr->reloc_count; r++) {
			struct flat_reloc reloc;

			fpos = hdr->reloc_start + (sizeof(struct flat_reloc)*r);
			result = bprm->file->f_op->read(bprm->file, (char *) &reloc,
					sizeof(reloc), &fpos);
			if (result >= (unsigned long)-4096) {
				printk("Failure reloading relocation\n");
			} else
				do_reloc(&reloc);
		}
	}

	compute_creds(bprm);
 	current->flags &= ~PF_FORKNOEXEC;

	flush_icache_range(current->mm->start_code, current->mm->end_code);

	set_binfmt(&flat_format);

	p = pos + data_len + bss_len + stack_len - 4;
	DBG_FLT("p=%x\n", p);

	p = putstringarray(p, 1, &bprm->filename);
	DBG_FLT("p(filename)=%x\n", p);

	p = putstringarray(p, bprm->envc, bprm->envp);
	DBG_FLT("p(envp)=%x\n", p);

	p = putstringarray(p, bprm->argc, bprm->argv);
	DBG_FLT("p(argv)=%x\n", p);

	current->mm->start_stack = (unsigned long) create_flat_tables(p, bprm);

	DBG_FLT("start_thread(regs=0x%x, start_code=0x%x, start_stack=0x%x)\n",
			regs, current->mm->start_code, current->mm->start_stack);
	start_thread(regs, current->mm->start_code, current->mm->start_stack);

	if (current->ptrace & PT_PTRACED)
		send_sig(SIGTRAP, current, 0);

	return 0;
}

static int load_flat_library(struct file *file)
{
	return(-ENOEXEC);
}

static int __init init_flat_binfmt(void)
{
	return register_binfmt(&flat_format);
}

static void __exit exit_flat_binfmt(void)
{
	unregister_binfmt(&flat_format);
}

EXPORT_NO_SYMBOLS;

module_init(init_flat_binfmt);
module_exit(exit_flat_binfmt);
