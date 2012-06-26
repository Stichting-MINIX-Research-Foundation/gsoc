LDADD+= -lfaultlib

.if ! empty(LLVM_CONF)

.include <bsd.own.mk>

.if "${LLVM_DEBUG}" != ""
LLVM_DEBUG_FLAG= -verbose-level=${LLVM_DEBUG}
.endif

LLVM.CC= llvmdrv ${LLVM_DEBUG_FLAG} -save-temps
LLVM.CXXCXX=${LLVM.CC}

.if "${LLVM_CONF}" == "TEST"
LLVM.CFLAGS=	-disable-pass=-codegenprepare -Wo-start -load=libLLVMHello.so -hello -Wo-end 
LLVM.LDFLAGS=	-Wo-start -load=libLLVMHello.so -hello -Wo-end -Wllld,-L/usr/lib/bca

.elif "${LLVM_CONF}" == "FAULT"
.if "${FAULT_FUNCS}" != ""
FAULT_FUNCS_ARG= -fault-functions ${FAULT_FUNCS}
.endif
LLVM.CFLAGS=	-disable-pass=-codegenprepare -disable-pass=-inline
LLVM.LDFLAGS=	-Wllld,-L/usr/lib/bca -Wo-start -load=libLLVMFaultInjector.so -faultinjector ${FAULT_FUNCS_ARG} -Wo-end

.elif "${LLVM_CONF}" == "NONE"
LLVM.CFLAGS=	-disable-pass=-codegenprepare
LLVM.LDFLAGS=	-Wllld,-L/usr/lib/bca

.else
.error unknown llvm pass: '${LLVM_CONF}'

.endif


.if defined(LIB)

COMPILE.c.o=${COMPILE.c:C/[[:<:]]${CC}[[:>:]]/${LLVM.CC}/g} ${LLVM.CFLAGS}

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
	echo build ${.TARGET} 
	${LLAR_CMD}

install: ${DESTDIR}${LIBDIR}/bca/lib${LIB}.bca

${DESTDIR}${LIBDIR}/bca/lib${LIB}.bca! lib${LIB}.bca __archiveinstall

clean:
	rm -f lib${LIB}.bca ${OBJS:.o=.bcc} ${OBJS:.o=.bccs.s} ${OBJS:.o=.BCC}

.else

CC=  ${LLVM.CC}
CXX= ${LLVM.CXX}

CFLAGS+=  ${LLVM.CFLAGS} 
LDFLAGS+= ${LLVM.LDFLAGS}

clean:
	rm -f ${OBJS:.o=.bcc} ${OBJS:.o=.bccs.s} ${OBJS:.o=.BCC} ${PROG}.bcl ${PROG}.BCL ${PROG}.bcl.sh ${PROG}.BCL.sh ${PROG}.bcls.s

.endif


.endif
