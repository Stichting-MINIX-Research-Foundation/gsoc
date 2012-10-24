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
#include <set>
#include <list>

#include "FaultInjector.h"
#include <sys/time.h>

using namespace llvm;

static cl::opt<int>
rand_seed("fault-rand-seed",
        cl::desc("Fault Injector: random seed value. when '0', current time is used. "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::list<std::string>
FunctionNames("fault-functions",
        cl::desc("Fault Injector: specify comma separated list of functions to be instrumented (empty = all functions)"), cl::NotHidden, cl::CommaSeparated);

static cl::list<std::string>
ModuleNames("fault-modules",
        cl::desc("Fault Injector: specify comma separated list of regular expressions to match modules (source file paths) to be instrumented (empty = all modules)"), cl::NotHidden, cl::CommaSeparated);

static cl::opt<int>
prob_global("fault-prob-global",
        cl::desc("Fault Injector: global dynamic fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

cl::opt<int>
prob_default("fault-prob-default",
        cl::desc("Fault Injector: default probability for all faults that are not specified. This also overrides indvidual default values."),
        cl::NotHidden);

static cl::opt<bool>
do_debug("fault-debug",
        cl::desc("Fault Injector: print debug information"),
        cl::init(0), cl::NotHidden);

bool isLibraryCompileUnit(DICompileUnit DICU)
{
    static bool regexesInitialized = false;
    static std::vector<Regex*> regexes;
    if(!regexesInitialized) {
        for (int i = 0; i < ModuleNames.size(); i++) {
            StringRef sr(ModuleNames[i]);
            Regex* regex = new Regex(sr);
            std::string error;
            assert(regex->isValid(error));
            regexes.push_back(regex);
        }    
        regexesInitialized = true;
    }    
    if(regexes.size() == 0) return true;
    for(unsigned i=0;i<regexes.size();i++) {
        if(regexes[i]->match(DICU.getDirectory(), NULL)) {
            return true;
        }    
    }
    return false;
}

namespace llvm{

    FaultInjector::FaultInjector() : ModulePass(ID) {}

    bool FaultInjector::runOnModule(Module &M) {

        /* seed rand() */
        if(rand_seed == 0){
            struct timeval tp;
            gettimeofday(&tp, NULL);
            srand((int) tp.tv_usec - tp.tv_sec);
        }else{
            srand(rand_seed);
        }

        GlobalVariable* enabled_var = M.getNamedGlobal("faultinjection_enabled");
        if(!enabled_var) {
            errs() << "Error: no faultinjection_enabled variable found";
            exit(1);
        }

        SmallVectorImpl<FaultType*> FaultTypes(0);
        FaultTypes.push_back(new SwapFault());
        FaultTypes.push_back(new NoLoadFault());
        FaultTypes.push_back(new RndLoadFault());
        FaultTypes.push_back(new NoStoreFault());
        FaultTypes.push_back(new FlipBranchFault());
        FaultTypes.push_back(new FlipBoolFault());
        FaultTypes.push_back(new CorruptPointerFault());
        FaultTypes.push_back(new CorruptIndexFault());
        FaultTypes.push_back(new CorruptIntegerFault());
        FaultTypes.push_back(new CorruptOperatorFault());

        for(std::vector<FaultType *>::size_type i = 0; i <  FaultTypes.size(); i++){
            FaultTypes[i]->addToModule(M);
        }

        for (Module::iterator it = M.getFunctionList().begin(); it != M.getFunctionList().end(); ++it) {
            Function *F = it;

            if(F->begin() == F->end()){
                // no basic blocks
                continue;
            }

            if(FunctionNames.size() > 0 && std::find (FunctionNames.begin(), FunctionNames.end(), F->getName()) == FunctionNames.end()){
                continue;
            }

            if(F->getName().equals("rand")){
                continue;
            }

            Value *DIF = Backports::findDbgSubprogramDeclare(F);
            if(DIF) {
                DISubprogram Func(cast<MDNode>(DIF));
                if(!isLibraryCompileUnit(Func.getCompileUnit())){
                    continue;   
                }
            }

            BasicBlock *OldFirstBB = F->getBasicBlockList().begin();
            BasicBlock *ClonedOldFirstBB = NULL;

            /* Create a new entrypoint */
            BasicBlock *NewFirstBB = BasicBlock::Create(M.getContext(), "newBB", F, OldFirstBB);

            /* Move all AllocaInstructions from OldFirstBB to NewFirstBB */
            for(BasicBlock::iterator BI = OldFirstBB->getInstList().begin(), BE = OldFirstBB->getInstList().end(); BI != BE;){
                Instruction *val = &(*BI++);
                if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(val)){
                    allocaInst->removeFromParent();
                    NewFirstBB->getInstList().push_back(allocaInst);
                }
            }

            SmallVectorImpl<BasicBlock*> Clones(0);
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
            ConstantInt* Constant1000 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1000"), 10));
            BinaryOperator *Remainder = BinaryOperator::Create(Instruction::SRem, RandFuncCall, Constant1000, "", RndBB); 
            /* remainder < tresshold? */
            ConstantInt* ConstantFaultPct = ConstantInt::get(M.getContext(), APInt(32, prob_global, 10));
            ICmpInst* do_cloned = new ICmpInst(*RndBB, ICmpInst::ICMP_ULE, Remainder, ConstantFaultPct, "");
            /* goto first cloned block or first original block */
            BranchInst::Create(ClonedOldFirstBB, OldFirstBB, do_cloned, RndBB);

            /* branch to original blocks or cloned blocks, based on value of enabled_var */
            LoadInst* load_enabled_var = new LoadInst(enabled_var, "", false, NewFirstBB);
            load_enabled_var->setAlignment(4);
            ConstantInt* Constant0 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));
            ICmpInst* do_rnd = new ICmpInst(*NewFirstBB, ICmpInst::ICMP_EQ, load_enabled_var, Constant0, "");
            BranchInst::Create(OldFirstBB, RndBB, do_rnd, NewFirstBB);

            /* loop through all cloned basic blocks */
            for(std::vector<BasicBlock>::size_type i = 0; i <  Clones.size(); i++){
                BasicBlock *BB = Clones[i];
                /* For each basic block, loop through all instructions */
                for (BasicBlock::iterator II = BB->begin(), nextII; II != BB->end();II=nextII){
                    Instruction *val = &(*II);

                    std::string tostr;
                    if(do_debug){
                        llvm::raw_string_ostream rsos(tostr);
                        val->print(rsos);
                        rsos.str();
                    }
                    
                    nextII=II;
                    nextII++;

                    /* rand even/odd predictability fix (first rand()%2 value predicts subsequent values on minix */
                    if(rand() > (RAND_MAX / 2)){
                        srand(rand());
                    }
                    
                    SmallVectorImpl<Value *> replacements(0);
                    FaultType *applied_fault_type = NULL;
                    bool removed = false;
                    for(std::vector<FaultType *>::size_type i = 0; i <  FaultTypes.size(); i++){
                        FaultType *FT = FaultTypes[i];
                        if(FT->isApplicable(val)){
                            if((rand() % 1000) < FT->getProbability()){
                                removed = FT->apply(val, &replacements);
                                applied_fault_type = FT;

                                /* Increase the stastistics counter for this fault */
                                ConstantInt* Constant1 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1"), 10));
                                LoadInst* count_var = new LoadInst(FT->getFaultCount(), "", false, nextII);
                                count_var->setAlignment(4);
                                BinaryOperator* count_incr = BinaryOperator::Create(Instruction::Add, count_var, Constant1, "", nextII);
                                StoreInst *store_count_var = new StoreInst(count_incr, FT->getFaultCount(), false, nextII);
                                store_count_var->setAlignment(4);
                                break;
                            }
                        }

                    }
                    if(do_debug){
                        if(applied_fault_type){
                            errs() << "   # applied fault " << applied_fault_type->getName() << "\n";
                            if(removed){
                                errs() << "-";
                            }else{
                                errs() << "*";
                            }
                            errs() << tostr << "\n";
                            for(std::vector<Value *>::size_type i = 0; i <  replacements.size(); i++){
                                errs() << "+" << *replacements[i] << "\n";
                            }

                        }else{
                            errs() << " " << tostr << "\n";
                        }
                    }

                }

                bool foundNonPhi = false;
                for (BasicBlock::iterator II = BB->begin(); II != BB->end();II++){
                    // if non-PHInodes have been inserted before PHINodes, the PHINodes have to be moved to the beginning of the BB
                    if(PHINode *PN = dyn_cast<PHINode>(II)){
                        // This is a PHINode
                        if(foundNonPhi){
                            // This is a PHINode, but we've already encountered a non-PHINode. Move PHINode to begin of BB
                            PN->moveBefore(BB->begin());
                            // restart the loop, to prevent messing up the iterator after moving it
                            II = BB->begin();
                            foundNonPhi = false;
                            continue;
                        }
                    }else{
                        // We found a non-PHINode. Any subsequent PHInode must be moved to begin of BB.
                        foundNonPhi = true;
                    }
                }

            }

        }

        /* Fix llvm 2.9 bug: llc finds duplicate debug symbols */
        Backports::StripDebugInfo(M);

        return true;
    }
}

char FaultInjector::ID = 0;
static RegisterPass<FaultInjector> X("faultinjector", "Fault Injector Pass");

