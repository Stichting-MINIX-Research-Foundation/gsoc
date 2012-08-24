#include <vector>
#include <string>

#include "llvm/Pass.h"
#include <llvm/Instructions.h>
#include <llvm/Module.h>

using namespace llvm;

namespace llvm {

    void StringExplode(std::string str, std::string separator, std::vector<std::string>* results);

    void count_incr(GlobalVariable *counter, Instruction *insertBefore, Module &M);
}

