# $Id: Makefile,v 1.1 2001/02/22 19:13:55 serg Exp $
# Makefile for the PlayStation prom monitor library routines
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

OBJS  = init.o memory.o cmdline.o identify.o

all: 	caetla.a

caetla.a: $(OBJS)
	$(AR) rcs caetla.a $(OBJS)
	sync

dep:
	$(CPP) $(CPPFLAGS) -M *.c > .depend

include $(TOPDIR)/Rules.make
