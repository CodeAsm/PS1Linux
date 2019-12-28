#
# Makefile for MIPS-specific library files..
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

L_TARGET = lib.a

obj-y				+= csum_partial.o csum_partial_copy.o \
				   rtc-std.o rtc-no.o memcpy.o memset.o \
				   watch.o strlen_user.o strncpy_user.o \
				   strnlen_user.o

ifdef CONFIG_CPU_R3000
  obj-y	+= r3k_dump_tlb.o
else 
  obj-y	+= dump_tlb.o
endif

include $(TOPDIR)/Rules.make
