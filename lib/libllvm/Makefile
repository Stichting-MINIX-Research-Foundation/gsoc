.include <bsd.own.mk>

INCLDIR!= llvm-config --includedir
GXX_LOC!= sh -c "which g++ 2> /dev/null ; echo ''"

.if exists($(INCLDIR)/llvm/Pass.h) && "$(GXX_LOC)" != ""
SUBDIR= hello
.endif

.include <bsd.subdir.mk>
