# Makefile for the mailbox driver.
PROG=	mailbox
SRCS=	mbox.c main.c

FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

.include <minix.service.mk>
