# $Id: Makefile,v 1.1 2001/03/28 12:33:03 serg Exp $
# Makefile for the PlayStation library routines
# under Linux.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

OBJS  = libpsx.o printgpu.o

all: 	libpsx.a

libpsx.a: $(OBJS)
	$(AR) rcs libpsx.a $(OBJS)
	sync

dep:
	$(CPP) $(CPPFLAGS) -M *.c > .depend

include $(TOPDIR)/Rules.make
