/*
 * linux/fs/binfmt_flat.c
 *
 *   Copyright (C) 2000 Lineo, by David McCullough <davidm@lineo.com>
 *  based heavily on:
 *
 *  linux/fs/binfmt_aout.c:
 *      Copyright (C) 1991, 1992, 1996  Linus Torvalds
 *  linux/fs/binfmt_flat.c for 2.0 kernel
 *       Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>
 *   JAN/99 -- coded full program relocation (gerg@lineo.com)
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
#define   DBG_FLT(a...)   printk(##a)
#else
#define   DBG_FLT(a...)
#endif

static int load_flat_binary(struct linux_binprm *, struct pt_regs * regs);
static int load_flat_library(struct file*);
static int flat_core_dump(long signr, struct pt_regs * regs, struct file *file);

extern void dump_thread(struct pt_regs *, struct user *);
extern int is_in_rom (unsigned long);

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
   DBG_FLT("putstringarray(0x%lx, %d, 0x%lx)\n", p, count, array);
   while (count) {
      p=putstring(p, array[--count]);
      DBG_FLT("p2=%lx\n", p);
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
do_reloc(flat_reloc * r)
{
   unsigned long offset;
   unsigned long * ptr;
   static unsigned long * prev_ptr;
   static flat_reloc prev;
   static int hi_flag = 0;
   static unsigned long res;
   long rel_offset;
   
   rel_offset = (*r & FLAT_RELOC_OFFSET_MASK);
   if ((*r & FLAT_RELOC_SIGN_MASK) == FLAT_RELOC_SIGN_NEG) rel_offset = -rel_offset;

   switch (*r & FLAT_RELOC_REL_MASK) {
      case FLAT_RELOC_REL_TEXT:
         offset = current->mm->start_code;
         break;
      case FLAT_RELOC_REL_DATA:
         offset = current->mm->start_data;
         break;
      case FLAT_RELOC_REL_BSS:
         offset= current->mm->end_data;
         break;
      default:
         printk("BINFMT_FLAT: Unknown relocation relay section=%d\n", (int)(*r & FLAT_RELOC_REL_MASK));
         return;
   }

   switch (*r & FLAT_RELOC_IN_MASK) {
      case FLAT_RELOC_IN_TEXT:
         ptr = (unsigned long*)(current->mm->start_code + rel_offset);
         break;
      case FLAT_RELOC_IN_DATA:
         ptr = (unsigned long*)(current->mm->start_data + rel_offset);
         break;
      case FLAT_RELOC_IN_BSS:
         ptr = (unsigned long*)(current->mm->end_data + rel_offset);
         break;
      default:
         printk("BINFMT_FLAT: Unknown relocation in section=%d\n", (int)(*r & FLAT_RELOC_IN_MASK));
         return;
   }
   
#if 0 // def DEBUG
   printk("Relocation = 0x%lx: type=%d rel_offset=0x%lx relay=%d in=%d offset=0x%lx addr=0x%lx [0x%lx->",
      (long)(*r), 
      (int)((*r & FLAT_RELOC_TYPE_MASK) >> FLAT_RELOC_TYPE_SHIFT), 
      (long)rel_offset, 
      (int)((*r & FLAT_RELOC_REL_MASK) >> FLAT_RELOC_REL_SHIFT), 
      (int)((*r & FLAT_RELOC_IN_MASK) >> FLAT_RELOC_IN_SHIFT),
      (unsigned long)offset,
      (unsigned long)ptr, 
      (unsigned long)(*ptr));
#endif

   switch (*r & FLAT_RELOC_TYPE_MASK) {
      case FLAT_RELOC_TYPE_32:
         *ptr += offset;
         break;
      case  FLAT_RELOC_TYPE_HI16:
         prev = *r;
         prev_ptr = ptr;
         res = (*ptr & 0xffff) << 16;
         hi_flag = 1;
#if 0 // def DEBUG
         printk("\n");
#endif
        return;
      case  FLAT_RELOC_TYPE_LO16:
         if (!hi_flag || 
            (*r & FLAT_RELOC_REL_MASK) != (prev & FLAT_RELOC_REL_MASK) ||  
            (*r & FLAT_RELOC_IN_MASK) != (prev & FLAT_RELOC_IN_MASK)) {
            printk ("\nBINFMT_FLAT: Bad reloc sequence (flag=%d prev: rel=%d, in=%d\n",
               (int)hi_flag, (int)(prev & FLAT_RELOC_REL_MASK), (int)(prev & FLAT_RELOC_IN_MASK));
            return;
         }
         res += (short)(*ptr & 0xffff);
         res += offset;
         *prev_ptr = (*prev_ptr & 0xffff0000) | (((res-(short)res) >> 16) & 0xffff);
         *ptr = (*ptr & 0xffff0000) | (res & 0xffff);
#if 0 // def DEBUG
         printk("0x%lx]\n", (long)(*ptr));
#endif
         ptr = prev_ptr;
         hi_flag = 0;
         break;
      case  FLAT_RELOC_TYPE_26:
         res = (*ptr & 0x3ffffff) << 2;
         res += offset;
         *ptr = (*ptr & 0xfc000000) | ((res >> 2) & 0x3ffffff);
         break;
      default:
         printk("\nBINFMT_FLAT: Unknown relocation type=%d\n", (int)(*r & FLAT_RELOC_TYPE_MASK));
         return;
   }

#if 0 // def DEBUG
   printk("0x%lx]\n", (long)(*ptr));
#endif
}



/*
 * These are the functions used to load a.out style executables and shared
 * libraries.  There is no binary dependent code anywhere else.
 */

static int load_flat_binary(struct linux_binprm * bprm, struct pt_regs * regs)
{
   struct flat_hdr * hdr;
   unsigned long result;
   unsigned long rlim;
   int retval;
   int r;

   unsigned long p = bprm->p;
   unsigned long data_len, bss_len, stack_len, code_len;
   unsigned long codep, datap, bssp;
   unsigned long memp = 0, memkasked = 0; /* for find brk area */
   struct inode *inode;
   loff_t fpos;

   DBG_FLT("BINFMT_FLAT: Loading file: %lx\n", bprm->file);

   hdr = ((struct flat_hdr *) bprm->buf);      /* exec-header */
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
#ifdef CONFIG_BINFMT_FLAT_SEPARATELY
   DBG_FLT ("BINFMT_FLAT: map code (len=0x%lx) to ", code_len);
   codep = do_mmap(bprm->file, 0, code_len,
         PROT_READ | PROT_EXEC | ((hdr->flags&FLAT_FLAG_RAM) ? PROT_WRITE : 0),
         0, 0);
   DBG_FLT ("0x%lx\n", codep);
   DBG_FLT ("BINFMT_FLAT: map data (len=0x%lx, offset=0x%lx) to ", data_len, hdr->data_start);
   datap = do_mmap(bprm->file, 0, data_len,
         PROT_READ | PROT_EXEC | PROT_WRITE,
         0, hdr->data_start);
   DBG_FLT ("0x%lx\n", datap);
   DBG_FLT ("BINFMT_FLAT: map data (len=0x%lx, offset=0x%lx) to ", bss_len+stack_len, hdr->data_end);
   bssp = do_mmap(bprm->file, 0, bss_len + stack_len,
         PROT_READ | PROT_EXEC | PROT_WRITE,
         0, hdr->data_end);
   DBG_FLT ("0x%lx\n", bssp);
#else
   codep = do_mmap(bprm->file, 0, code_len + data_len + bss_len + stack_len,
         PROT_READ|PROT_EXEC | ((hdr->flags&FLAT_FLAG_RAM) ? PROT_WRITE : 0),
         0, 0);
   datap = codep+code_len;
   bssp = datap+data_len;
#endif
   up(&current->mm->mmap_sem);
        
   if (codep >= -4096) {
      printk("Unable to map flat executable code section, errno %d\n", (int)-codep);
      return codep; /* Beyond point of no return? Oh well... */
   }
        
#ifdef CONFIG_BINFMT_FLAT_SEPARATELY
   if (datap >= -4096) {
      do_munmap(current->mm, codep, 0);
      printk("Unable to map flat executable data section, errno %d\n", (int)-datap);
      return datap; /* Beyond point of no return? Oh well... */
   }

   if (bssp >= -4096) {
      do_munmap(current->mm, codep, 0);
      do_munmap(current->mm, datap, 0);
      printk("Unable to map flat executable bss section, errno %d\n", (int)-bssp);
      return bssp; /* Beyond point of no return? Oh well... */
   }
#endif

#ifdef CONFIG_BINFMT_FLAT_SEPARATELY
   memp = bssp;
   memkasked = bss_len + stack_len;
#else
   memp = codep;
   memkasked = code_len + data_len + bss_len + stack_len;
#endif

   DBG_FLT("BINFMT_FLAT: mmap returned: %lx\n", error);

   current->mm->executable = NULL;
   
   if (is_in_rom(codep)) {
   	
#ifndef CONFIG_BINFMT_FLAT_SEPARATELY
      /*
       * do_mmap returned a ROM mapping, so allocate RAM for
       * data + bss + stack
       */
      unsigned long result;

      DBG_FLT("BINFMT_FLAT: ROM mapping of file\n");

      down(&current->mm->mmap_sem);
      datap = do_mmap(0, 0, data_len + bss_len + stack_len,
            PROT_READ|PROT_WRITE|PROT_EXEC, 0, 0);
      up(&current->mm->mmap_sem);
      if (datap >= (unsigned long)-4096) {
         printk("Unable to allocate RAM for process, errno %d\n", (int)-datap);
         return datap;
      }
      
      bssp = datap+data_len;

      memp = datap; /* don't use ROM mmapping for sbrk!, use tail of data */
      memkasked = data_len + bss_len + stack_len;
      
      DBG_FLT("BINFMT_FLAT: Allocated data+bss+stack (%ld bytes): %lx\n",
            data_len + bss_len + stack_len, datap);

      /* And then fill it in */

      fpos = hdr->data_start;
      result = bprm->file->f_op->read(bprm->file, (char *) datap, data_len, &fpos);
      if (result >= (unsigned long)-4096) {
         do_munmap(current->mm, datap, 0);
         printk("Unable to read data+bss, errno %d\n", (int)-result);
         send_sig(SIGKILL, current, 0);
         return result;
      }
#endif

      if (IS_SYNC(inode)) {
         DBG_FLT("Retaining inode\n");
         current->mm->executable = bprm->file->f_dentry->d_inode;
         atomic_inc(&inode->i_count);
      }
   }

   /* zero the BSS */
   memset((void*)bssp, 0, bss_len + stack_len);

   DBG_FLT("Entry point is %lx, text_start is %lx, data_start is %lx\n",
         codep+hdr->entry, codep, datap);

   current->mm->start_code = codep + hdr->text_start;
   current->mm->end_code = codep + code_len;
   current->mm->start_data = datap;
   current->mm->end_data = datap + data_len;
#ifdef NO_MM
   /*
    *   set up the brk stuff (uses any slack left in data/bss allocation
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

#ifndef CONFIG_BINFMT_FLAT_SEPARATELY
   if (is_in_rom(codep)) {
      for(r = 0; r < hdr->reloc_count; r++) {
         flat_reloc * reloc = (flat_reloc*)
            (codep + hdr->reloc_start + (sizeof(flat_reloc)*r));
         do_reloc(reloc);
      }
   } else {
#endif
      for(r = 0; r < hdr->reloc_count; r++) {
         flat_reloc reloc;

         fpos = hdr->reloc_start + (sizeof(flat_reloc)*r);
         result = bprm->file->f_op->read(bprm->file, (char *) &reloc,
               sizeof(reloc), &fpos);
         if (result >= (unsigned long)-4096) {
            printk("Failure reloading relocation\n");
         } else
            do_reloc(&reloc);
      }
#ifndef CONFIG_BINFMT_FLAT_SEPARATELY
   }
#endif

#if 0 // def DEBUG
   printk ("load_flat: relocs=%d\n", (int)(hdr->reloc_count));
#endif

   compute_creds(bprm);
    current->flags &= ~PF_FORKNOEXEC;

   flush_icache_range(current->mm->start_code, current->mm->end_code);

   set_binfmt(&flat_format);

   p = bssp + bss_len + stack_len - 4;
   DBG_FLT("p=%lx\n", p);

   p = putstringarray(p, 1, &bprm->filename);
   DBG_FLT("p(filename)=%lx\n", p);

   p = putstringarray(p, bprm->envc, bprm->envp);
   DBG_FLT("p(envp)=%lx\n", p);

   p = putstringarray(p, bprm->argc, bprm->argv);
   DBG_FLT("p(argv)=%lx\n", p);

   current->mm->start_stack = (unsigned long) create_flat_tables(p, bprm);

   DBG_FLT("start_thread(regs=0x%lx, entry=0x%lx, start_stack=0x%lx)\n",
         (long)regs, (long)(current->mm->start_code+hdr->entry_point),
         (long)(current->mm->start_stack));

   start_thread(regs, current->mm->start_code+hdr->entry_point, current->mm->start_stack);

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
