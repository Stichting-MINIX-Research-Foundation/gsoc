.if "${LIB}"!="faultlib"

WARNS=0
USE_SHLIBDIR=	yes

LLVM_CONF=

CC := g++
CXX := g++

CXXFLAGS :=-I/usr/pkg/include -D_MINIX -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -O3 -fomit-frame-pointer -fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual    -pedantic -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings

CXXFLAGS := ${CXXFLAGS} -I/usr/pkg/gcc44/include/c++/4.4.6 -I/usr/pkg/gcc44/include/c++/4.4.6/i386-pc-minix

CPPFLAGS := ${CXXFLAGS}

LDFLAGS :=-D_POSIX_SOURCE -D_MINIX -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -O3 -fomit-frame-pointer -fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual  -lcompat_minix -lminlib -pedantic -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings  -O3 -shared -lm

.endif
