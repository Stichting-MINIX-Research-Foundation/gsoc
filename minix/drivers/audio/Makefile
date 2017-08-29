.include <bsd.own.mk>

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	es1370
SUBDIR+=	es1371
SUBDIR+=	sb16
.endif # ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
SUBDIR+=	pcm
SUBDIR+=	pwmjack
.endif # ${MACHINE_ARCH} == "earm"
.include <bsd.subdir.mk>
