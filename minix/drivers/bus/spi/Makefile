# Makefile for the SPI broadcom driver
PROG=	spi
SRCS=	spi.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+= ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -lchardriver -lsys

.include <minix.service.mk>