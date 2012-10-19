#ifndef BACKPORTS_H
#define BACKPORTS_H

#include "llvm/Pass.h"
#include <llvm/Instructions.h>
#include <llvm/Module.h>
#include <llvm/IntrinsicInst.h>
#include <llvm/Analysis/DebugInfo.h>

using namespace llvm;

namespace llvm {

class Backports {
  public:

      //From DbgInfoPrinter.cpp (LLVM 2.9)
      static Value *findDbgGlobalDeclare(GlobalVariable *V);
      static Value *findDbgSubprogramDeclare(Function *V);
      static const DbgDeclareInst *findDbgDeclare(const Value *V);
      static bool getLocationInfo(const Value *V, std::string &DisplayName,
                            std::string &Type, unsigned &LineNo,
                            std::string &File, std::string &Dir);

      //From llvm/lib/Transforms/IPO/StripSymbols.cpp (LLVM 2.9)
      static bool StripDebugInfo(Module &M);

};

}

#endif
