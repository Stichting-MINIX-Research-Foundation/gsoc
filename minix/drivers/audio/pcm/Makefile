# Makefile for the PCM broadcom driver
PROG=	pcm
SRCS=	pcm.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
