########################################################################
# $Id: Makefile,v 1.4 2000/01/13 22:41:41 kili Exp $
#
# Toplevel makefile for `cm'.
#
# $Log: Makefile,v $
# Revision 1.4  2000/01/13 22:41:41  kili
# Just a check-in with dependencies included into Makefiles.
#
# Revision 1.3  1996/03/03 13:14:10  kilian
# checked in with -k by kilian at 1996/04/01 19:10:17
#
# Revision 1.3  1996/03/03  13:14:10  kilian
# Added target `install'.
#
# Revision 1.2  1996/02/12  14:36:29  kilian
# Added program `midiplay'.
#
# Revision 1.1  1996/02/05  16:14:09  kilian
# Initial revision
#
#
########################################################################
include Rules

# Programs to build:
PROGS     = mito

# Sub directories:
SUBDIRS   = include lib

all:: $(PROGS)

install:: all
	for i in $(PROGS); do $(INSTALLBIN) $$i $(BINDIR); done

mito:					mito.o -lmidi

# DO NOT DELETE
# AUTOMATICALLY GENERATED DEPENDENCIES
mito.o: mito.c include/midi.h include/print.h include/buffer.h \
 include/chunk.h include/event.h include/score.h include/track.h \
 include/util.h include/vld.h
