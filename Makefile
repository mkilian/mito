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
mito.o: mito.c include/chunk.h include/buffer.h include/event.h \
  include/print.h include/score.h include/track.h include/util.h \
  include/vld.h
