include $(TOP)/Rules

# Libraries to build:
LIBS      = libmidi.a

# Several groups of object files:
OBJS	=	buffer.o chunk.o event.o print.o score.o track.o vld.o util.o

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

# DO NOT DELETE
# AUTOMATICALLY GENERATED DEPENDENCIES
buffer.o: buffer.c ../include/buffer.h
chunk.o: chunk.c ../include/print.h ../include/chunk.h \
 ../include/buffer.h
event.o: event.c ../include/print.h ../include/event.h \
 ../include/buffer.h ../include/vld.h
print.o: print.c ../include/print.h
score.o: score.c ../include/print.h ../include/chunk.h \
 ../include/buffer.h ../include/score.h ../include/event.h \
 ../include/track.h
track.o: track.c ../include/track.h ../include/event.h \
 ../include/buffer.h
vld.o: vld.c ../include/print.h ../include/vld.h ../include/buffer.h
util.o: util.c ../include/util.h ../include/track.h ../include/event.h \
 ../include/buffer.h
