# Makefile for I2C support

PROG= i2c
SRCS+= i2c.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

.include <bsd.own.mk>

.if ${MACHINE_ARCH} == "earm"
.for pl in rpi omap
.PATH: ${.CURDIR}/arch/earm/${pl}
SRCS+= ${pl}_i2c.c
.endfor
.else
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS+= pci_i2c.c
.endif

CPPFLAGS+= -I${.CURDIR} -I${.CURDIR}/arch/earm/rpi -I${.CURDIR}/arch/earm/omap
DPADD+= ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS} ${LIBCLKCONF}
LDADD+= -lchardriver -lsys -ltimers -lclkconf

.include <minix.service.mk>