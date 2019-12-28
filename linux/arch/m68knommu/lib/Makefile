#
# Makefile for m68k-specific library files..
#

.S.o:
	$(CC) $(AFLAGS) -D__ASSEMBLY__ -traditional -c $< -o $@

L_TARGET = lib.a
obj-y  = ashrdi3.o checksum.o semaphore.o memcpy.o memset.o

include $(TOPDIR)/Rules.make
