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
#include <sys/time.h>

using namespace llvm;

static cl::opt<std::string>
FaultFunctions("fault-functions",
        cl::desc("Fault Injector: specify comma separated list of functions to be instrumented (empty = all functions)"),
        cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_global("fault-prob-global",
        cl::desc("Fault Injector: global dynamic fault probability (0 - 1000) "),
        cl::init(1000), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_swap("fault-prob-swap",
        cl::desc("Fault Injector: binary operand swap fault probability (0 - 1000) "),
        cl::init(1000), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_no_load("fault-prob-no-load",
        cl::desc("Fault Injector: load instruction loading '0' fault probability (0 - 1000) "),
        cl::init(1000), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_no_store("fault-prob-no-store",
        cl::desc("Fault Injector: remove store instruction fault probability (0 - 1000) "),
        cl::init(1000), cl::NotHidden, cl::ValueRequired);

static cl::opt<bool>
SwapBinary("fault-swap-binary",
        cl::desc("Fault Injector: Swaps binary operands"),
        cl::init(true), cl::NotHidden, cl::ValueRequired);


namespace llvm{

    FaultInjector::FaultInjector() : ModulePass(ID) {}

    bool FaultInjector::runOnModule(Module &M) {

        /* seed rand() */
        struct timeval tp;
        gettimeofday(&tp, NULL);
        srand((int) tp.tv_usec - tp.tv_sec);

        std::vector<std::string> FunctionNames(0);
        StringExplode(FaultFunctions, ",", &FunctionNames);

        Module::FunctionListType &functionList = M.getFunctionList();
        SmallVectorImpl<BasicBlock*> Clones(0);

        ConstantInt* Constant0 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));
        ConstantInt* Constant1000 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1000"), 10));
        ConstantInt* ConstantFaultPct = ConstantInt::get(M.getContext(), APInt(32, prob_global, 10));

        GlobalVariable* enabled_var = M.getNamedGlobal("faultinjection_enabled");
        if(!enabled_var) {
            errs() << "Error: no faultinjection_enabled variable found";
            exit(1);
        }
#if 0
        enabled_var->setInitializer(ConstantInt::get(M.getContext(), APInt(32, 1)));
#endif                    

        GlobalVariable* fault_count_swap_var = M.getNamedGlobal("fault_count_swap");
        GlobalVariable* fault_count_no_load_var = M.getNamedGlobal("fault_count_no_load");
        GlobalVariable* fault_count_no_store_var = M.getNamedGlobal("fault_count_no_store");
        if(!fault_count_swap_var || !fault_count_no_load_var || !fault_count_no_store_var) {
            errs() << "Error: no fault counter variable found";
            exit(1);
        }

        GlobalVariable* GV_int_0 = new GlobalVariable(/*Module=*/M, 
                /*Type=*/IntegerType::get(M.getContext(), 32),
                /*isConstant=*/false,
                /*Linkage=*/GlobalValue::ExternalLinkage,
                /*Initializer=*/0, // has initializer, specified below
                /*Name=*/"GV_int_0");
        GV_int_0->setAlignment(4);
        GV_int_0->setInitializer(Constant0);

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
            /* take rand() % 1000 */
            BinaryOperator *Remainder = BinaryOperator::Create(Instruction::SRem, RandFuncCall, Constant1000, "", RndBB); 
            /* remainder < tresshold? */
            ICmpInst* do_cloned = new ICmpInst(*RndBB, ICmpInst::ICMP_ULE, Remainder, ConstantFaultPct, "");
            /* goto first cloned block or first original block */
            BranchInst::Create(ClonedOldFirstBB, OldFirstBB, do_cloned, RndBB);

            /* branch to original blocks or cloned blocks, based on value of enabled_var */
            LoadInst* load_enabled_var = new LoadInst(enabled_var, "", false, NewFirstBB);
            load_enabled_var->setAlignment(4);
            ICmpInst* do_rnd = new ICmpInst(*NewFirstBB, ICmpInst::ICMP_EQ, load_enabled_var, Constant0, "");
            BranchInst::Create(OldFirstBB, RndBB, do_rnd, NewFirstBB);

            /* loop through all cloned basic blocks */
            for(std::vector<BasicBlock>::size_type i = 0; i <  Clones.size(); i++){
                BasicBlock *BB = Clones[i];
                /* For each basic block, loop through all instructions */
                for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II){
                    Instruction *inst = &(*II);
                    errs() << "> ";
                    inst->print(errs());
                    errs() << "\n";
                    bool removed = false;
                    if (SwapBinary){
                        /* switch operands of binary instructions */
                        if(BinaryOperator *Op = dyn_cast<BinaryOperator>(inst)){
                            if((rand() % 1000) < prob_swap){
                                Value *tmp = Op->getOperand(0);
                                Op->setOperand(0, Op->getOperand(1));
                                Op->setOperand(1, tmp);

                                count_incr(fault_count_swap_var, Op, M);
                            }
                        }
                    }
                    if (1 /* loadNull */){
                        if(LoadInst *LI = dyn_cast<LoadInst>(inst)){
                            if(LI->getOperand(0)->getType()->getContainedType(0)->isIntegerTy()){
                                if((rand() % 1000) < prob_no_load){
                                    LI->setOperand(0, GV_int_0);
                                    count_incr(fault_count_no_load_var, LI, M);
                                }
                            }
                        }
                    }
                    if (1 /* delStore */){
                        if(StoreInst *SI = dyn_cast<StoreInst>(inst)){
                            if((rand() % 1000) < prob_no_store){
                                --II; /* decrease iterator, so that it can be incremented for the next iteration */
                                count_incr(fault_count_no_store_var, II, M);
                                SI->eraseFromParent();
                                removed=true;
                            }
                        }
                    }
                    errs() << "< ";
                    if(removed){
                        errs() << "<removed>\n";
                    }else{
                        inst->print(errs());
                        errs() << "\n";
                    }

                }
            }

        }

        return true;
    }
}

char FaultInjector::ID = 0;
static RegisterPass<FaultInjector> X("faultinjector", "Fault Injector Pass");

