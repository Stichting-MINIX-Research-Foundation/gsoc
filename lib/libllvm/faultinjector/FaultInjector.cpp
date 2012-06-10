#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct FaultInjector : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    FaultInjector() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      errs() << "FaultInjector: ";
      errs().write_escaped(F.getName()) << '\n';
      return false;
    }
  };
}

char FaultInjector::ID = 0;
static RegisterPass<FaultInjector> X("faultinjector", "Fault Injector Pass");

