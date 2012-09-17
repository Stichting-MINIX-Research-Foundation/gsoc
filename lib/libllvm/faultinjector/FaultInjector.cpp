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
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_swap("fault-prob-swap",
        cl::desc("Fault Injector: binary operand swap fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_no_load("fault-prob-no-load",
        cl::desc("Fault Injector: load instruction loading '0' fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_rnd_load("fault-prob-rnd-load",
        cl::desc("Fault Injector: load instruction loading 'rnd()' fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_no_store("fault-prob-no-store",
        cl::desc("Fault Injector: remove store instruction fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_flip_bool("fault-prob-flip-bool",
        cl::desc("Fault Injector: flip boolean value fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_flip_branch("fault-prob-flip-branch",
        cl::desc("Fault Injector: flip branch fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_corrupt_pointer("fault-prob-corrupt-pointer",
        cl::desc("Fault Injector: corrupt pointer fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_corrupt_integer("fault-prob-corrupt-integer",
        cl::desc("Fault Injector: corrupt integer fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);


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

        ConstantInt* True = ConstantInt::get(M.getContext(), APInt(1, StringRef("-1"), 10));
        ConstantInt* Constant0 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));
        ConstantInt* Constant1 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1"), 10));
        ConstantInt* Constant1000 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1000"), 10));
        ConstantInt* ConstantFaultPct = ConstantInt::get(M.getContext(), APInt(32, prob_global, 10));

        GlobalVariable* enabled_var = M.getNamedGlobal("faultinjection_enabled");
        if(!enabled_var) {
            errs() << "Error: no faultinjection_enabled variable found";
            exit(1);
        }

        GlobalVariable* fault_count_swap_var = M.getNamedGlobal("fault_count_swap");
        GlobalVariable* fault_count_no_load_var = M.getNamedGlobal("fault_count_no_load");
        GlobalVariable* fault_count_rnd_load_var = M.getNamedGlobal("fault_count_rnd_load");
        GlobalVariable* fault_count_no_store_var = M.getNamedGlobal("fault_count_no_store");
        GlobalVariable* fault_count_flip_bool_var = M.getNamedGlobal("fault_count_flip_bool");
        GlobalVariable* fault_count_flip_branch_var = M.getNamedGlobal("fault_count_flip_branch");
        GlobalVariable* fault_count_corrupt_pointer_var = M.getNamedGlobal("fault_count_corrupt_pointer");
        GlobalVariable* fault_count_corrupt_integer_var = M.getNamedGlobal("fault_count_corrupt_integer");
        if(!fault_count_swap_var || !fault_count_no_load_var || !fault_count_rnd_load_var || !fault_count_no_store_var || !fault_count_flip_bool_var || !fault_count_flip_branch_var || !fault_count_corrupt_pointer_var || !fault_count_corrupt_integer_var) {
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
                Instruction *val = &(*BI++);
                if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(val)){
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
                for (BasicBlock::iterator II = BB->begin(), nextII; II != BB->end();II=nextII){
                    Value *val = &(*II);
                    errs() << "> ";
                    val->print(errs());
                    errs() << "\n";
                    bool removed = false;
                    nextII=II;
                    nextII++;
            
                    do{        
                        if(BinaryOperator *Op = dyn_cast<BinaryOperator>(val)){
                            if((rand() % 1000) < prob_swap){
                                /* switch operands of binary instructions */
                                /* todo: if(op1.type == op2.type */
                                Value *tmp = Op->getOperand(0);
                                Op->setOperand(0, Op->getOperand(1));
                                Op->setOperand(1, tmp);

                                count_incr(fault_count_swap_var, Op, M);
                                continue;
                            }
                        }

                        if(LoadInst *LI = dyn_cast<LoadInst>(val)){
                            if(LI->getOperand(0)->getType()->getContainedType(0)->isIntegerTy()){
                                Value *newValue;
                                bool doReplace = false;
                                if((rand() % 1000) < prob_no_load){
                                    /* load 0 instead of target value. */
                                    newValue = Constant::getNullValue(LI->getOperand(0)->getType()->getContainedType(0));
                                    count_incr(fault_count_no_load_var, nextII, M);
                                    doReplace=true;
                                }else if((rand() % 1000) < prob_rnd_load){
                                    if(val->getType()->isIntegerTy(32)){
                                        newValue = CallInst::Create(RandFunc, "", nextII);
                                        count_incr(fault_count_rnd_load_var, nextII, M);
                                        doReplace=true;
                                    }
                                }
                                
                                if(doReplace){
                                    LI->replaceAllUsesWith(newValue);
                                    LI->eraseFromParent();
                                    val = newValue;
                                    continue;
                                }
                            }
                        }

                        if(StoreInst *SI = dyn_cast<StoreInst>(val)){
                            if((rand() % 1000) < prob_no_store){
                                /* remove store instruction */
                                count_incr(fault_count_no_store_var, II, M);
                                SI->eraseFromParent();
                                removed=true;
                                continue;
                            }
                        }

                        if(val->getType()->isIntegerTy(1)){
                            
                            if((rand() % 1000) < prob_flip_branch){
                                /* boolean value is used as branching condition */
                                BranchInst *BI;
                                bool isBranchInst = false;
                                for(Value::use_iterator U = val->use_begin(), UE = val->use_end(); U != UE; U++){
                                    if ((BI = dyn_cast<BranchInst>(*U))) {
                                        isBranchInst = true;
                                        break;
                                    }
                                }
                                if(isBranchInst){
                                    errs() << "Negated branch instruction\n";
                                    count_incr(fault_count_flip_branch_var, nextII, M);
                                    BinaryOperator* Negation = BinaryOperator::Create(Instruction::Xor, val, True, "", nextII);
                                    BI->setOperand(0, Negation);
                                    continue;
                                }
                            }

                            if((rand() % 1000) < prob_flip_bool){
                                /* boolean value, not necessarily used as branching condition */
                                count_incr(fault_count_flip_bool_var, nextII, M);

                                BinaryOperator* Negation = BinaryOperator::Create(Instruction::Xor, val, True, "", nextII);
                                II->replaceAllUsesWith(Negation);
                                Negation->setOperand(0, II); /* restore the use in the negation instruction */
                                errs() << "< inserted (after next): ";
                                Negation->print(errs());
                                errs() << "\n";

                                continue;
                            }
                        
                        }

                        if(val->getType()->isPointerTy()){
                            if((rand() % 1000) < prob_corrupt_pointer){
                                CallInst* PtrRandFuncCall = CallInst::Create(RandFunc, "", nextII);
                                CastInst* rand64 = new SExtInst(PtrRandFuncCall, IntegerType::get(M.getContext(), 64), "", nextII);
                                CastInst* rnd_ptr = new IntToPtrInst(rand64, val->getType(), "", nextII);    
                                II->replaceAllUsesWith(rnd_ptr);
                                count_incr(fault_count_corrupt_pointer_var, nextII, M);
                                continue;
                            }
                        }

                        if(val->getType()->isIntegerTy(32)){
                            if((rand() % 1000) < prob_corrupt_integer){
                                Instruction::BinaryOps opcode;
                                if(rand() % 2){
                                    opcode=Instruction::Add;
                                }else{
                                    opcode=Instruction::Sub;
                                }
                                BinaryOperator* Change = BinaryOperator::Create(opcode, val, Constant1, "", nextII);
                                II->replaceAllUsesWith(Change);
                                Change->setOperand(0, II); /* restore the use in the change instruction */
                                count_incr(fault_count_corrupt_integer_var, nextII, M);
                                continue;
                            }
                        }

                    }
                    while(false);

                    errs() << "< ";
                    if(removed){
                        errs() << "<removed>\n";
                    }else{
                        val->print(errs());
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

