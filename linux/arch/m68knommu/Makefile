#
# m68knommu/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (c) 1999,2001  D. Jeff Dionne <jeff@lineo.ca>,
#	                   Rt-Control Inc. / Lineo, Inc.
#
# Copyright (C) 1998,1999  D. Jeff Dionne <jeff@uclinux.org>,
#                          Kenneth Albanowski <kjahds@kjahds.com>,
#
# Based on arch/m68k/Makefile:
# Copyright (C) 1994 by Hamish Macdonald
#

# test for cross compiling
COMPILE_ARCH = $(shell uname -m)

UTS_SYSNAME = -DUTS_SYSNAME=\"uClinux\"

# Find out which board we are compiling for
include arch/$(ARCH)/Boards.mk

# Set up the memory model.  RAM or ROM.
ifdef CONFIG_RAMKERNEL
MODEL = ram
else
MODEL = rom
endif
export MODEL

# get the compiler, flags and targets from the platform
ifdef PLATFORM
include arch/$(ARCH)/platform/$(PLATFORM)/Rules.make
endif

arch/$(ARCH)/empty.o:
	$(CROSS_COMPILE)gcc -o arch/$(ARCH)/empty.o -c arch/$(ARCH)/empty.c

bootstrap:
	@$(MAKEBOOT) bootstrap

archmrproper:

archdep:
