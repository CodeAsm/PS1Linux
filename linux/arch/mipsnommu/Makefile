#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994, 1995, 1996 by Ralf Baechle
# DECStation modifications by Paul M. Antoine, 1996
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#

#
# Select the object file format to substitute into the linker script.
#
ifdef CONFIG_CPU_LITTLE_ENDIAN
tool-prefix	= mipsel-linux-
output-format	= elf32-littlemips
else
tool-prefix	= mips-linux-
output-format	= elf32-bigmips
endif

ifdef CONFIG_CROSSCOMPILE
CROSS_COMPILE  = $(tool-prefix)
endif

#
# GCC uses -G0 -mabicalls -fpic as default.  We don't want PIC in the kernel
# code since it only slows down the whole thing.  At some point we might make
# use of global pointer optimizations but their use of $28 conflicts with
# the current pointer optimization.
#
# The DECStation requires an ECOFF kernel for remote booting, other MIPS
# machines may also.  Since BFD is incredibly buggy with respect to
# crossformat linking we rely on the elf2ecoff tool for format conversion.
#
GCCFLAGS	:= -G 0 -mno-abicalls -fno-pic
LINKFLAGS	+= -static -G 0
MODFLAGS	+= -mlong-calls

ifdef CONFIG_REMOTE_DEBUG
CFLAGS		:= $(CFLAGS) -g
endif

#
# CPU-dependent compiler/assembler options for optimization.
#
ifdef CONFIG_CPU_R3000
GCCFLAGS	+= -mcpu=r3000 -mips1
endif
ifdef CONFIG_CPU_R6000
GCCFLAGS	+= -mcpu=r6000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R4300
GCCFLAGS	+= -mcpu=r4300 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R4X00
GCCFLAGS	+= -mcpu=r4600 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_MIPS32
GCCFLAGS	+= -mcpu=r4600 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R5000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R5432
GCCFLAGS        += -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_NEVADA
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap -mmad
endif
ifdef CONFIG_CPU_RM7000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R8000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif
ifdef CONFIG_CPU_R10000
GCCFLAGS	+= -mcpu=r8000 -mips2 -Wa,--trap
endif

ifdef CONFIG_MIPS_FPU_EMULATOR
CORE_FILES	+=arch/mipsnommu/math-emu/fpu_emulator.o
SUBDIRS		+=arch/mipsnommu/math-emu
endif

#
# The pipe options is bad for my low-mem machine
# Uncomment this if you want this.
#
GCCFLAGS	+= -pipe

CFLAGS		:= -I $(TOPDIR)/include/asm/gcc $(CFLAGS) $(GCCFLAGS)
AFLAGS		+= $(GCCFLAGS)

#
# Board-dependent options and extra files
#
#
# PlayStation
#
ifdef CONFIG_PLAYSTATION
CORE_FILES    += arch/mipsnommu/ps/ps.o
SUBDIRS       += arch/mipsnommu/ps arch/mipsnommu/ps/prom
LIBS          += arch/mipsnommu/ps/prom/prom.a
LOADADDR      += 0x80010000
ifdef CONFIG_PS_GPU_CONSOLE 
SUBDIRS	      += arch/mipsnommu/ps/libpsx
LIBS          += arch/mipsnommu/ps/libpsx/libpsx.a
endif
ifndef CONFIG_PS_GPU_CONSOLE
ifdef CONFIG_GPUPSX_CONSOLE
SUBDIRS	      += arch/mipsnommu/ps/libpsx
LIBS          += arch/mipsnommu/ps/libpsx/libpsx.a
endif
endif
endif

#
# Choosing incompatible machines durings configuration will result in
# error messages during linking.  Select a default linkscript if
# none has been choosen above.
#
linux: arch/$(ARCH)/ld.script

arch/$(ARCH)/ld.script: arch/$(ARCH)/ld.script.in arch/$(ARCH)/Makefile
	sed -e 's/@@OUTPUT_FORMAT@@/$(output-format)/' \
	    -e 's/@@LOADADDR@@/$(LOADADDR)/' <$< >$@
LINKFLAGS	+= -T arch/$(ARCH)/ld.script

HEAD := arch/mipsnommu/kernel/head.o arch/mipsnommu/kernel/init_task.o

SUBDIRS := $(addprefix arch/mipsnommu/, tools) $(SUBDIRS) $(addprefix arch/mipsnommu/, kernel mm lib)
CORE_FILES := arch/mipsnommu/kernel/kernel.o arch/mipsnommu/mm/mm.o $(CORE_FILES)
LIBS := arch/mipsnommu/lib/lib.a $(LIBS)

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

vmlinux.ecoff: vmlinux
	@$(MAKEBOOT) $@

zImage: vmlinux
	@$(MAKEBOOT) zImage

compressed: zImage

zdisk: vmlinux
	@$(MAKEBOOT) zdisk

archclean:
	@$(MAKEBOOT) clean
	rm -f arch/$(ARCH)/ld.script
	$(MAKE) -C arch/$(ARCH)/tools clean

archmrproper:
	@$(MAKEBOOT) mrproper
	$(MAKE) -C arch/$(ARCH)/tools mrproper

archdep:
	if [ ! -f $(TOPDIR)/include/asm-$(ARCH)/offset.h ]; then \
	 touch $(TOPDIR)/include/asm-$(ARCH)/offset.h; \
	fi;
	@$(MAKEBOOT) dep
