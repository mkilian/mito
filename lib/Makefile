########################################################################
# $Id: Makefile,v 1.2 1996/04/06 23:02:51 kilian Exp $
#
# Makefile for `cm/lib'.
#
# $Log: Makefile,v $
# Revision 1.2  1996/04/06 23:02:51  kilian
# Added track.o.
#
# Revision 1.1  1996/02/05  16:09:37  kilian
# Initial revision
#
#
########################################################################
include $(TOP)/Rules

# Libraries to build:
LIBS      = libmidi.a

# Several groups of object files:
OBJS	=	buffer.o chunk.o event.o print.o score.o track.o vld.o

all:: $(LIBS)

libmidi.a:  libmidi.a($(OBJS))

# Any idea how to make such rules `magic', i.e. put a generic rule into
# `$(TOP)/Rules' that will build libraries automatically?
# Something like
#   `lib%.a: lib%.a($%OBJS)'
# would be nice, since one just could say
#    LIBS = libfoo.a libbar.a
#    fooOBJS = foo1.o foo2.o
#    barOBJS = bar1.o bar2.o
# and make would do the job.
# However, make does not understand the `$%OBJS' in the rule above.
# Maybe, that some tricky substitutions would make this kind of magic
# possible.

