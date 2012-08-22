#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"


#include <llvm/Transforms/Utils/ValueMapper.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>

#include <assert.h>

#include <algorithm>
#include <vector>

#include "FaultInjector.h"
#include "FaultUtil.h"

using namespace llvm;

static cl::opt<std::string>
FaultFunctions("fault-functions",
        cl::desc("Fault Injector: specify comma separated list of functions to be instrumented (empty = all functions)"),
        cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
Pct("fault-pct",
        cl::desc("Fault Injector: fault probability (0 - 100) "),
        cl::init(100), cl::NotHidden, cl::ValueRequired);

static cl::opt<bool>
SwapBinary("fault-swap-binary",
        cl::desc("Fault Injector: Swaps binary operands"),
        cl::init(true), cl::NotHidden, cl::ValueRequired);


namespace llvm{

    FaultInjector::FaultInjector() : ModulePass(ID) {}

    bool FaultInjector::runOnModule(Module &M) {

        std::vector<std::string> FunctionNames(0);
        StringExplode(FaultFunctions, ",", &FunctionNames);

        Module::FunctionListType &functionList = M.getFunctionList();
        SmallVectorImpl<BasicBlock*> Clones(0);

        GlobalVariable* enabled_var = M.getNamedGlobal("faultinjection_enabled");
        if(!enabled_var) {
            errs() << "Error: no faultinjection_enabled variable found";
            exit(1);
        }
#if 0
        enabled_var->setInitializer(ConstantInt::get(M.getContext(), APInt(32, 1)));
#endif                    

        ConstantInt* Constant0 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));
        ConstantInt* Constant100 = ConstantInt::get(M.getContext(), APInt(32, StringRef("100"), 10));
        ConstantInt* ConstantFaultPct = ConstantInt::get(M.getContext(), APInt(32, Pct, 10));


        for (Module::iterator it = functionList.begin(); it != functionList.end(); ++it) {
            Function *F = it;

            if(FunctionNames.size() > 0 && std::find (FunctionNames.begin(), FunctionNames.end(), F->getName()) == FunctionNames.end()){
                continue;
            }

            if(F->begin() == F->end()){
                // no basic blocks
                continue;
            }

            BasicBlock *OldFirstBB = F->getBasicBlockList().begin();
            BasicBlock *ClonedOldFirstBB = NULL;

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

                for (Function::const_iterator BI = ++F->begin(), BE = F->end(); BI != BE; ++BI) {
                    const BasicBlock &BB = *BI;
                    if(std::find (Clones.begin(), Clones.end(), &BB) == Clones.end()){

                        // Create a new basic block and copy instructions into it!
                        BasicBlock *CBB = CloneBasicBlock(&BB, VMap, ".CLONED", F, NULL);
                        VMap[&BB] = CBB;                       // Add basic block mapping.
                        Clones.push_back(CBB);
                        if(!ClonedOldFirstBB){
                            ClonedOldFirstBB = CBB;
                        }
                    }

                }

                // Loop over all of the instructions in the function, fixing up operand
                // references as we go.  This uses VMap to do all the hard work.
                for (Function::iterator BB = F->begin(), BE = F->end(); BB != BE; ++BB)
                    if(std::find (Clones.begin(), Clones.end(), BB) != Clones.end())
                        // Loop over all instructions, fixing each one as we find it...
                        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II)
                            RemapInstruction(II, VMap, RF_NoModuleLevelChanges);

            }

            /* Insert a basic block before the cloned basic blocks that will execute the cloned or original blocks, based on randomness */
            BasicBlock *RndBB = BasicBlock::Create(M.getContext(), "RndBB", F, ClonedOldFirstBB);

            /* Call rand() */
            Function *RandFunc = M.getFunction("rand");
            assert(RandFunc);
            CallInst* RandFuncCall = CallInst::Create(RandFunc, "", RndBB);
            /* take rand() % 100 */
            BinaryOperator *Remainder = BinaryOperator::Create(Instruction::SRem, RandFuncCall, Constant100, "", RndBB); 
            /* remainder < tresshold? */
            ICmpInst* do_cloned = new ICmpInst(*RndBB, ICmpInst::ICMP_ULE, Remainder, ConstantFaultPct, "");
            /* goto first cloned block or first original block */
            BranchInst::Create(ClonedOldFirstBB, OldFirstBB, do_cloned, RndBB);

            /* branch to original blocks or cloned blocks, based on value of enabled_var */
            LoadInst* load_enabled_var = new LoadInst(enabled_var, "", false, NewFirstBB);
            load_enabled_var->setAlignment(4);
            ICmpInst* do_rnd = new ICmpInst(*NewFirstBB, ICmpInst::ICMP_EQ, load_enabled_var, Constant0, "");
            BranchInst::Create(OldFirstBB, RndBB, do_rnd, NewFirstBB);

            /* loop through all cloned basic blocks, and switch operands of binary instructions */
            for(std::vector<BasicBlock>::size_type i = 0; i <  Clones.size(); i++){
                BasicBlock *BB = Clones[i];
                for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II){
                    Instruction *inst = &(*II);
                    if (SwapBinary){
                        if(BinaryOperator *Op = dyn_cast<BinaryOperator>(inst)){
                            Value *tmp = Op->getOperand(0);
                            Op->setOperand(0, Op->getOperand(1));
                            Op->setOperand(1, tmp);
                        }
                    }
                }
            }

#if 0
            /* Add a printf("cloned\n") call to the first cloned basic block */

            ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(M.getContext(), 8), 8);

            GlobalVariable* gvar_array__str = new GlobalVariable(/*Module=*/M, 
                    /*Type=*/ArrayTy_0,
                    /*isConstant=*/true,
                    /*Linkage=*/GlobalValue::PrivateLinkage,
                    /*Initializer=*/0, // has initializer, specified below
                    /*Name=*/".str");
            gvar_array__str->setAlignment(1);

            Constant* const_array_6 = ConstantArray::get(M.getContext(), "cloned\x0A", true);
            std::vector<Constant*> const_ptr_7_indices;
            ConstantInt* const_int32_8 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));
            const_ptr_7_indices.push_back(const_int32_8);
            const_ptr_7_indices.push_back(const_int32_8);
            gvar_array__str->setInitializer(const_array_6);
            Constant* const_ptr_7 = ConstantExpr::getGetElementPtr(gvar_array__str, &const_ptr_7_indices[0], 2, true);


            Function* func_printf = M.getFunction("printf");
            CallInst::Create(func_printf, const_ptr_7, "", ClonedOldFirstBB->begin());
#endif
        }

        return true;
    }
}

char FaultInjector::ID = 0;
static RegisterPass<FaultInjector> X("faultinjector", "Fault Injector Pass");

