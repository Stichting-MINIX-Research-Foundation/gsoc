#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"


#include <llvm/Transforms/Utils/ValueMapper.h>
#include <llvm/Transforms/Utils/Cloning.h>

#include <algorithm>
#include <vector>

using namespace llvm;

namespace {
    struct FaultInjector : public FunctionPass {
        static char ID; // Pass identification, replacement for typeid
        FaultInjector() : FunctionPass(ID) {}

        virtual bool runOnFunction(Function &F) {
            return cloneBasicBlocks(F);
        }

        virtual bool cloneBasicBlocks(Function &F) {

            ValueToValueMapTy VMap;
            SmallVectorImpl<BasicBlock*> Clones(0);

            errs() << "FaultInjector: ";
            errs().write_escaped(F.getName()) << '\n';

            // clone code inspired by llvm::CloneFunctionInto() (llvm/lib/Transforms/Utils/CloneFunction.cpp)

            if(F.begin() != F.end()){
                for (Function::const_iterator BI = F.begin()++, BE = F.end(); BI != BE; ++BI) {
                    const BasicBlock &BB = *BI;
                    if(std::find (Clones.begin(), Clones.end(), &BB) == Clones.end()){

                        // Create a new basic block and copy instructions into it!
                        BasicBlock *CBB = CloneBasicBlock(&BB, VMap, ".CLONED", &F, NULL);
                        VMap[&BB] = CBB;                       // Add basic block mapping.
                        Clones.push_back(CBB);
                        errs() << "FaultInjector:          created basic block " << CBB->getName() << "\n";
                    }

                }
            }
            
            // Loop over all of the instructions in the function, fixing up operand
            // references as we go.  This uses VMap to do all the hard work.
            for (Function::iterator BB = F.begin(), BE = F.end(); BB != BE; ++BB)
                if(std::find (Clones.begin(), Clones.end(), BB) != Clones.end())
                    // Loop over all instructions, fixing each one as we find it...
                    for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II)
                        //TODO: if variable is not in VMap (is in 1st BB or in arg_list), will the value not be changed, as expected?
                        RemapInstruction(II, VMap, RF_NoModuleLevelChanges);

            return true;
        }
    };
}

char FaultInjector::ID = 0;
static RegisterPass<FaultInjector> X("faultinjector", "Fault Injector Pass");

