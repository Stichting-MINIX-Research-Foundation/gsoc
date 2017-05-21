.include <bsd.init.mk>

SRCDIST= ${NETBSDSRCDIR}/external/bsd/libfdt/dist
SRCDIR= ${NETBSDSRCDIR}/external/bsd/libfdt

LIB=	fdt

SRCS=	fdt.c fdt_ro.c fdt_wip.c fdt_sw.c fdt_rw.c fdt_strerror.c fdt_empty_tree.c \
	fdt_addresses.c fdt_overlay.c

.PATH: ${SRCDIST} ${SRCDIST}/include

.include <bsd.own.mk>

CPPFLAGS+=	-I${SRCDIST} -I${SRCDIR}/include -I${SRCDIST}/include

.include <bsd.lib.mk>