# $Id: Makefile,v 1.2 2001/03/19 13:13:45 serg Exp $
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

all: 	prom.a

prom.a: $(OBJS)
	$(AR) rcs prom.a $(OBJS)
	sync

dep:
	$(CPP) $(CPPFLAGS) -M *.c > .depend

include $(TOPDIR)/Rules.make
