PROG=	mito
SRCS=	mito.c buffer.c chunk.c event.c print.c score.c track.c util.c \
	vld.c
MAN=

LDADD=	-lsndio
DPADD=	${LIBSNDIO}

.include <bsd.prog.mk>
