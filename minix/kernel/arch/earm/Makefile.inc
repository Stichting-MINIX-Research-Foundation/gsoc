# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}

# LSC: putchar and kputc have the same signature. A bit hackish.
CPPFLAGS.subr_prf.c+= -Dputchar=kputc

# Activate optional support, may be deactivated.
CPPFLAGS.subr_prf.c+= -DLIBSA_PRINTF_LONGLONG_SUPPORT -DLIBSA_PRINTF_WIDTH_SUPPORT

.include "bsp/${BSP_NAME}/Makefile.inc"

# some object files we give a symbol prefix (or namespace) of __k_unpaged_
# that must live in their own unique namespace.
#
.for unpaged_obj in head.o
unpaged_${unpaged_obj}: ${unpaged_obj}
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${.OBJDIR}/${unpaged_obj} $@
UNPAGED_OBJS += unpaged_${unpaged_obj}
ORIG_UNPAGED_OBJS += ${unpaged_obj}
.endfor


CLEANFILES+= ${ORIG_UNPAGED_OBJS}

SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c do_padconf.c \
	exception.c hw_intr.c klib.S memory.c pre_init.c \
	protect.c direct_tty_utils.c arch_reset.c \
	pg_utils.c phys_copy.S phys_memset.S exc.S

OBJS.kernel+=	${UNPAGED_OBJS}

klib.o mpx.o head.o: procoffsets.h

SRCS+= procoffsets.h

PROCOFFSETSCF=procoffsets.cf

.PATH: ${NETBSDSRCDIR}/minix/include/arch/${MACHINE_ARCH}/include

procoffsets.h: ${PROCOFFSETSCF} kernel.h proc.h stackframe.h archtypes.h
	${_MKTARGET_CREATE}
	${TOOL_CAT} ${HERE}/${PROCOFFSETSCF} | \
		${TOOL_GENASSYM} -- ${CC} ${CFLAGS:N-Wa,*} \
		${CPPFLAGS} ${PROF} ${${USE_BITCODE:Uno} == "yes":? -fno-lto:} \
		${GENASSYM_CPPFLAGS} >$@.tmp && \
	  mv -f $@.tmp $@ 

sconst.h: procoffsets.h
apic_asm.o head.o klib.o mpx.o: sconst.h

