# Makefile for I2C support

.if ${MACHINE_ARCH} == "earm"
.for pl in rpi omap
.PATH: ${.CURDIR}/arch/earm/${pl}
PROGS+= i2c_${pl}
SRCS.i2c_${pl}+= i2c.c ${pl}_i2c.c ${pl}_i2c.h ${pl}_i2c_registers.h
FILES.i2c_${pl}=i2c_${pl}.conf
FILESNAME=i2c_${pl}
.endfor
.else
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS.i2c += pci_i2c.c pci_i2c.h pci_i2c_register.h
.endif

FILESDIR= /etc/system.conf.d
CPPFLAGS+= -I${.CURDIR} -I${.CURDIR}/arch/earm/rpi -I${.CURDIR}/arch/earm/omap
DPADD+= ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS} ${LIBCLKCONF}
LDADD+= -lchardriver -lsys -ltimers -lclkconf

MKMAN:= no

.include <minix.service.mk>