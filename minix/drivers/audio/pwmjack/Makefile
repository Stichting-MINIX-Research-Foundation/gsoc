# Makefile for the PCM broadcom driver
PROG=	pwmjack
SRCS=	jack.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
