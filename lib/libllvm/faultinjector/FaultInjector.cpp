#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"


#include <llvm/Transforms/Utils/ValueMapper.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>

#include <assert.h>

#include <algorithm>
#include <vector>

#include "FaultInjector.h"

using namespace llvm;

namespace llvm{

    char ID; // Pass identification, replacement for typeid
    FaultInjector::FaultInjector() : ModulePass(ID) {}

    bool FaultInjector::runOnModule(Module &M) {

        Module::FunctionListType &functionList = M.getFunctionList();

        for (Module::iterator it = functionList.begin(); it != functionList.end(); ++it) {
            Function *F = it;

            if(F->begin() == F->end()){
                // no basic blocks
                continue;
            }

            BasicBlock *OldFirstBB = F->getBasicBlockList().begin();

            /* Create a new entrypoint */
            BasicBlock *NewFirstBB = BasicBlock::Create(M.getContext(), "newBB", F, OldFirstBB);

            /* Move all AllocaInstructions from OldFirstBB to NewFirstBB */
            for(BasicBlock::iterator BI = OldFirstBB->getInstList().begin(), BE = OldFirstBB->getInstList().end(); BI != BE;){
                Instruction *inst = &(*BI++);
                if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(inst)){
                    allocaInst->removeFromParent();
                    NewFirstBB->getInstList().push_back(allocaInst);
                }
            }

            // clone code inspired by llvm::CloneFunctionInto() (llvm/lib/Transforms/Utils/CloneFunction.cpp)
            {
                ValueToValueMapTy VMap;
                SmallVectorImpl<BasicBlock*> Clones(0);

                for (Function::const_iterator BI = ++F->begin(), BE = F->end(); BI != BE; ++BI) {
                    const BasicBlock &BB = *BI;
                    if(std::find (Clones.begin(), Clones.end(), &BB) == Clones.end()){

                        // Create a new basic block and copy instructions into it!
                        BasicBlock *CBB = CloneBasicBlock(&BB, VMap, ".CLONED", F, NULL);
                        VMap[&BB] = CBB;                       // Add basic block mapping.
                        Clones.push_back(CBB);
                    }

                }

                // Loop over all of the instructions in the function, fixing up operand
                // references as we go.  This uses VMap to do all the hard work.
                for (Function::iterator BB = F->begin(), BE = F->end(); BB != BE; ++BB)
                    if(std::find (Clones.begin(), Clones.end(), BB) != Clones.end())
                        // Loop over all instructions, fixing each one as we find it...
                        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II)
                            //TODO: if variable is not in VMap (is in 1st BB or in arg_list), will the value not be changed, as expected?
                            RemapInstruction(II, VMap, RF_NoModuleLevelChanges);

            }

            /* Branch from the new to the old first block */
            BranchInst *br =  BranchInst::Create (OldFirstBB, NewFirstBB);
            br->setSuccessor(0, OldFirstBB); 

        }

        GlobalVariable* enabled_var = M.getNamedGlobal("faultinjection_enabled");
        if(!enabled_var) {
            errs() << "Error: no faultinjection_enabled variable found";
            exit(1);
        }
        enabled_var->setInitializer(ConstantInt::get(M.getContext(), APInt(32, 1)));
                    

        return true;
    }
}

char FaultInjector::ID = 0;
static RegisterPass<FaultInjector> X("faultinjector", "Fault Injector Pass");

