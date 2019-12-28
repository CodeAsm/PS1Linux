#
# Makefile for the Linux/MIPS kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(AFLAGS) $< -o $@
.S.o:
	$(CC) $(AFLAGS) -c $< -o $@

EXTRA_ASFLAGS = -mips3 -mcpu=r4000

all:	kernel.o head.o init_task.o

O_TARGET := kernel.o

obj-y				+= branch.o process.o signal.o entry.o \
				   traps.o ptrace.o vm86.o ioport.o reset.o \
				   semaphore.o setup.o syscall.o sysmips.o \
				   ipc.o scall_o32.o unaligned.o
obj-$(CONFIG_MODULES)		+= mips_ksyms.o

ifdef CONFIG_CPU_R3000
obj-y	+= r2300_misc.o r2300_fpu.o r2300_switch.o
else
obj-y	+= r4k_misc.o r4k_switch.o 
ifdef CONFIG_CPU_R6000
obj-y	+= r6000_fpu.o
else
obj-y	+= r4k_fpu.o
endif
endif

obj-$(CONFIG_MIPS_FPE_MODULE)	+= fpe.o
obj-$(CONFIG_MIPS_FPU_EMULATOR)	+= softfp.o

# Old style irq support, going to die in 2.5.
export-objs			+= old-irq.o
obj-$(CONFIG_ROTTEN_IRQ)	+= old-irq.o

ifndef CONFIG_DECSTATION
  ifndef CONFIG_BAGET_MIPS
    ifndef CONFIG_MIPS_ATLAS
      ifndef CONFIG_MIPS_MALTA
        ifndef CONFIG_MIPS_EV96100
	  ifndef CONFIG_PLAYSTATION
	    obj-y += time.o
	  endif
        endif
      endif
    endif
  endif
endif

obj-$(CONFIG_BINFMT_IRIX)	+= irixelf.o irixioctl.o irixsig.o sysirix.o \
				   irixinv.o
obj-$(CONFIG_REMOTE_DEBUG)	+= gdb-low.o gdb-stub.o 
obj-$(CONFIG_PCI)		+= pci-dma.o
obj-$(CONFIG_PROC_FS)		+= proc.o

entry.o: entry.S
head.o: head.S

include $(TOPDIR)/Rules.make
