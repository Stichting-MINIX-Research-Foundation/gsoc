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

#include <llvm/Support/CFG.h>
#include <llvm/Transforms/Utils/Local.h>

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

            SmallVectorImpl<BasicBlock*> Headers(0), CorruptedClones(0);
            // clone code inspired by llvm::CloneFunctionInto() (llvm/lib/Transforms/Utils/CloneFunction.cpp)
            {
                ValueToValueMapTy VMap;

                for (Function::const_iterator BI = ++F->begin(), BE = F->end(); BI != BE; ++BI) {
                    const BasicBlock &BB = *BI;
                    if(std::find (Headers.begin(), Headers.end(), &BB) == Headers.end()){

                        // Create a new basic block and copy instructions into it!
                        BasicBlock *CBB = CloneBasicBlock(&BB, VMap, ".CLONED", F, NULL);
                        VMap[&BB] = CBB;                       // Add basic block mapping.
                        Headers.push_back(CBB);
                        if(!ClonedOldFirstBB){
                            ClonedOldFirstBB = CBB;
                        }
                    }

                }

                // Loop over all of the instructions in the function, fixing up operand
                // references as we go.  This uses VMap to do all the hard work.
                for (Function::iterator BB = F->begin(), BE = F->end(); BB != BE; ++BB)
                    if(std::find (Headers.begin(), Headers.end(), BB) != Headers.end())
                        // Loop over all instructions, fixing each one as we find it...
                        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II)
                            RemapInstruction(II, VMap, RF_NoModuleLevelChanges);

            }

            /* branch to original blocks or cloned blocks, based on value of enabled_var */
            LoadInst* load_enabled_var = new LoadInst(enabled_var, "", false, NewFirstBB);
            load_enabled_var->setAlignment(4);
            ConstantInt* Constant0 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));
            ICmpInst* do_rnd = new ICmpInst(*NewFirstBB, ICmpInst::ICMP_EQ, load_enabled_var, Constant0, "");
            BranchInst::Create(OldFirstBB, ClonedOldFirstBB, do_rnd, NewFirstBB);
            
            /* Because basic blocks will be cloned, users of an instruction outside of the same basic block 
             * are no longer dominated by the instruction. Therefore, move values to the stack,
             * and work with alloca, load and store instructions. After creating the cloned basic blocks,
             * the values can be promoted back to virtual registers by the mem2reg optimization pass.
             * Alternatively, a complex network of PHINodes has to be built, in order to restore dominance over users.
             * */
            {
                for(std::vector<BasicBlock>::size_type i = 0; i <  Headers.size(); i++){
                    BasicBlock *Header = Headers[i];
                    for (BasicBlock::iterator II = Header->begin(); II != Header->end(); ++II){
                        /* Try to find a user that will no longer be dominated after cloning. */
                        for (Value::use_iterator i = II->use_begin(), e = II->use_end(); i != e; ++i){
                            bool doDemote = true;
                            if(PHINode *P = dyn_cast<PHINode>(*i)){
                                /* Only find PHINodes that are not in a direct successor to the current basic block */
                                for (succ_iterator SI = succ_begin(Header), E = succ_end(Header); SI != E; ++SI) {
                                    if(*SI == P->getParent()){
                                        /* This PHINode is in a direct successor */
                                        doDemote = false;
                                        break;
                                    }
                                }
                            }else if(dyn_cast<Instruction>(*i)->getParent() == Header){
                                /* This user is located in the same basic block */
                                doDemote = false;
                            }
                            assert(dyn_cast<Instruction>(II));

                            if(doDemote){
                                /* Convert this instruction and its users to use alloca, load and store instructions */
                                DemoteRegToStack(*dyn_cast<Instruction>(II), false, NULL);
                                II = Header->begin();
                                break;
                            }
                        }
                    }
                }
            }

            
            /* Now split the cloned BBs into headers of phinodes, and the rest of the BB body 
             * The body is cloned again, so that we get both a correct and a corrupt clone.
             * The header branches to one of the cloned versions, based on probability */ 
            {
                for(std::vector<BasicBlock>::size_type i = 0; i <  Headers.size(); i++){
                    BasicBlock *Header = Headers[i];
                    ValueToValueMapTy VMap;

                    /* Move PHINodes into a header basic block */
                    BasicBlock *Corrupted = Header->splitBasicBlock(Header->getFirstNonPHI());
                    CorruptedClones.push_back(Corrupted);

                    BasicBlock *Correct = CloneBasicBlock(Corrupted, VMap, ".Correct", F, NULL);
                    VMap[Corrupted] = Correct;                       // Add basic block mapping.
                    for (BasicBlock::iterator II = Correct->begin(); II != Correct->end(); ++II)
                        RemapInstruction(II, VMap, RF_NoModuleLevelChanges);
                    Correct->moveBefore(Corrupted);

                    /* Update PHINodes in direct successor basic blocks. 
                     * They already contain incoming values for the corrupt clone, 
                     * so we now add the cloned value from the correct clone.
                     *
                     * See also:
                     *      BasicBlock::splitBasicBlock (last part of function)
                     *      BasicBlock::replaceSuccessorsPhiUsesWith() (seems to do same as split)
                     * */
                    for (succ_iterator I=succ_begin(Corrupted), E=succ_end(Corrupted); I!=E; ++I) {
                        BasicBlock *Successor = *I;
                        PHINode *PN;
                        for (BasicBlock::iterator II = Successor->begin();(PN = dyn_cast<PHINode>(II)); ++II) {
                            Value *CorruptedValue = PN->getIncomingValueForBlock(Corrupted);
                            Value *NewValue;
                            if(VMap[CorruptedValue]){
                                /* value is mapped */
                                NewValue = VMap[CorruptedValue];
                            }else{
                                /* value is not mapped.
                                 * Its probably located in a predecessor block, 
                                 * and not yet demoted
                                 * */
                                NewValue = CorruptedValue;
                            }
                            PN->addIncoming(NewValue, Correct);
                        }
                    }

                    /* Replace the unconditional branch instr. with branch based on probability */
                    Header->getTerminator()->eraseFromParent();
                    
                    /* Call rand() */
                    Function *RandFunc = M.getFunction("rand");
                    assert(RandFunc);
                    CallInst* RandFuncCall = CallInst::Create(RandFunc, "", Header);
                    /* take rand() % 1000 */
                    ConstantInt* Constant1000 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1000"), 10));
                    BinaryOperator *Remainder = BinaryOperator::Create(Instruction::SRem, RandFuncCall, Constant1000, "", Header); 
                    /* remainder < tresshold? */
                    ConstantInt* ConstantFaultPct = ConstantInt::get(M.getContext(), APInt(32, prob_global, 10));
                    ICmpInst* do_cloned = new ICmpInst(*Header, ICmpInst::ICMP_ULE, Remainder, ConstantFaultPct, "");
                    /* goto first cloned block or first original block */
                    BranchInst::Create(Corrupted, Correct, do_cloned, Header);


                }
            }

            /* loop through all cloned basic blocks */
            for(std::vector<BasicBlock>::size_type i = 0; i <  CorruptedClones.size(); i++){
                BasicBlock *BB = CorruptedClones[i];
                /* For each basic block, loop through all instructions */
                for (BasicBlock::iterator II = BB->begin(), nextII; II != BB->end();II=nextII){
                    Instruction *val = &(*II);

                    nextII=II;
                    nextII++;

                    if(val->getName().find(".reload") != StringRef::npos){
                        continue;
                    }

                    std::string tostr;
                    if(do_debug){
                        llvm::raw_string_ostream rsos(tostr);
                        val->print(rsos);
                        rsos.str();
                    }
                    
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

            }

        }

        /* Fix llvm 2.9 bug: llc finds duplicate debug symbols */
        Backports::StripDebugInfo(M);

        return true;
    }
}

char FaultInjector::ID = 0;
static RegisterPass<FaultInjector> X("faultinjector", "Fault Injector Pass");

