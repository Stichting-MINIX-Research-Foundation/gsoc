#include "Fault.h"
#include "Backports.h"
#include <llvm/Support/Regex.h>

using namespace llvm;

namespace llvm {

class FaultInjector : public ModulePass {

  public:
      static char ID;

      FaultInjector();

      virtual bool runOnModule(Module &M);

};

}

