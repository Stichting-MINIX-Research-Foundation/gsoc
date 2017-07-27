# Makefile for the SPI broadcom driver
PROG=	spi
SRCS=	spi.c

FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+= ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -lchardriver -lsys

# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

.include <minix.service.mk>