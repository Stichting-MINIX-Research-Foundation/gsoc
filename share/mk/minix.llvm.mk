.if ! empty(LLVM_CONF)

.include <bsd.own.mk>

.if "${LLVM_DEBUG}" != ""
LLVM_DEBUG_FLAG= -verbose-level=${LLVM_DEBUG}
.endif

CC= llvmdrv ${LLVM_DEBUG_FLAG} 
CXX=${CC}

.if defined(LIB)

MKPIC:=		no

LLAR= llvm-ar

BCC_SRCS ?= ${SRCS:N*.h:N*.sh:N*.S}
.if ! empty(BCC_SRCS)
BCC_OBJS ?= ${BCC_SRCS:R:S/$/.bcc/g}
.endif
LLAR_CMD= ${LLAR} ${_ARFL} ${.TARGET} ${BCC_OBJS}

lib${LIB}.a:: lib${LIB}.bca

lib${LIB}.bca: ${OBJS}
	# should depend on BCC_OBJS, but BCC_OBJS might be a shell command
	# see for example lib/libm/Makefile
	#echo BC_SRCS: ${BCC_SRCS}
	#echo ALL_TARGETS: ${.ALLTARGETS:M*.S}
	echo build ${.TARGET} 
	echo BCC_SRCS: ${BCC_SRCS}
	echo ${LLAR_CMD}
	${LLAR_CMD}

install: ${DESTDIR}${LIBDIR}/bca/lib${LIB}.bca

${DESTDIR}${LIBDIR}/bca/lib${LIB}.bca! lib${LIB}.bca __archiveinstall

clean:
	rm -f lib${LIB}.bca ${OBJS:.o=.bcc}

.else

clean:
	rm -f ${OBJS:.o=.bcc} ${PROG}.bcl ${PROG}.BCL ${PROG}.bcl.sh ${PROG}.BCL.sh ${PROG}.bcls.s

.endif

.if "${LLVM_CONF}" == "TEST"
CFLAGS+= -disable-pass=-codegenprepare -Wo-start -load=libLLVMHello.so -hello -Wo-end 
LDFLAGS+= -Wo-start -load=libLLVMHello.so -hello -Wo-end -Wllld,-L/usr/lib/bca
.endif

.endif
