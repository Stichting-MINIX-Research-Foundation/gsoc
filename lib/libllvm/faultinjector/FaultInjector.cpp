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

static cl::opt<int>
rand_seed("fault-rand-seed",
        cl::desc("Fault Injector: random seed value. when '0', current time is used. "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<std::string>
FaultFunctions("fault-functions",
        cl::desc("Fault Injector: specify comma separated list of functions to be instrumented (empty = all functions)"),
        cl::init(""), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_global("fault-prob-global",
        cl::desc("Fault Injector: global dynamic fault probability (0 - 1000) "),
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

static cl::opt<int>
prob_corrupt_index("fault-prob-corrupt-index",
        cl::desc("Fault Injector: corrupt index fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

static cl::opt<int>
prob_corrupt_operator("fault-prob-corrupt-operator",
        cl::desc("Fault Injector: corrupt binary operator fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);


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

        GlobalVariable* fault_count_flip_bool_var = M.getNamedGlobal("fault_count_flip_bool");
        GlobalVariable* fault_count_flip_branch_var = M.getNamedGlobal("fault_count_flip_branch");
        GlobalVariable* fault_count_corrupt_pointer_var = M.getNamedGlobal("fault_count_corrupt_pointer");
        GlobalVariable* fault_count_corrupt_integer_var = M.getNamedGlobal("fault_count_corrupt_integer");
        GlobalVariable* fault_count_corrupt_index_var = M.getNamedGlobal("fault_count_corrupt_index");
        GlobalVariable* fault_count_corrupt_operator_var = M.getNamedGlobal("fault_count_corrupt_operator");
        if(!fault_count_flip_bool_var || !fault_count_flip_branch_var || !fault_count_corrupt_pointer_var || !fault_count_corrupt_integer_var || !fault_count_corrupt_index_var || !fault_count_corrupt_operator_var) {
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


        SwapFault FS;
        FS.addToModule(M);

        NoLoadFault NL;
        NL.addToModule(M);

        RndLoadFault RL;
        RL.addToModule(M);

        NoStoreFault NS;
        NS.addToModule(M);

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
                    Instruction *val = &(*II);
                    errs() << "> ";
                    val->print(errs());
                    errs() << "\n";
                    nextII=II;
                    nextII++;

                    /* rand even/odd predictability fix (first rand()%2 value predicts subsequent values on minix */
                    if(rand() > (RAND_MAX / 2)){
                        srand(rand());
                    }
            
                    do{        
                        if(FS.isApplicable(val)){
                            if((rand() % 1000) < FS.getProbability()){
                                val = FS.apply(val);
                                count_incr(FS.fault_count, nextII, M);
                                continue;
                            }
                        }

                        if(NL.isApplicable(val)){
                            if((rand() % 1000) < NL.getProbability()){
                                val = NL.apply(val);
                                count_incr(NL.fault_count, nextII, M);
                                continue;
                            }
                        }
                        
                        if(RL.isApplicable(val)){
                            if((rand() % 1000) < RL.getProbability()){
                                val = RL.apply(val);
                                count_incr(RL.fault_count, nextII, M);
                                continue;
                            }
                        } 

                        if(NS.isApplicable(val)){
                            if((rand() % 1000) < NS.getProbability()){
                                val = NS.apply(val);
                                count_incr(NS.fault_count, nextII, M);
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
                            
                            bool doCorrupt = false;

                            if((rand() % 1000) < prob_corrupt_index){
                                GetElementPtrInst *GEP;
                                for(Value::use_iterator U = val->use_begin(), UE = val->use_end(); U != UE; U++){
                                    if ((GEP = dyn_cast<GetElementPtrInst>(*U))) {
                                        doCorrupt = true;
                                        count_incr(fault_count_corrupt_index_var, nextII, M);
                                        break;
                                    }
                                }
                            }

                            if(!doCorrupt && (rand() % 1000) < prob_corrupt_integer){
                                doCorrupt = true;
                                count_incr(fault_count_corrupt_integer_var, nextII, M);
                            }

                            if(doCorrupt){
                                Instruction::BinaryOps opcode;
                                if(rand() % 2){
                                    opcode=Instruction::Add;
                                }else{
                                    opcode=Instruction::Sub;
                                }
                                BinaryOperator* Change = BinaryOperator::Create(opcode, val, Constant1, "", nextII);
                                II->replaceAllUsesWith(Change);
                                Change->setOperand(0, II); /* restore the use in the change instruction */
                                continue;
                            }
                        }

                        if(BinaryOperator *BO = dyn_cast<BinaryOperator>(val)){
                            if((rand() % 1000) < prob_corrupt_operator){
                                
                                Instruction::BinaryOps newOpCode;
                                
                                if(BO->getType()->isIntOrIntVectorTy()){
                                    Instruction::BinaryOps IntOps[] = {BinaryOperator::Add, BinaryOperator::Sub, BinaryOperator::Mul, BinaryOperator::UDiv, BinaryOperator::SDiv, BinaryOperator::URem, BinaryOperator::SRem, BinaryOperator::Shl, BinaryOperator::LShr, BinaryOperator::AShr, BinaryOperator::And, BinaryOperator::Or, BinaryOperator::Xor};
                                    newOpCode = IntOps[rand() % (sizeof(IntOps)/sizeof(IntOps[0]))];
                                }else{
                                    assert(BO->getType()->isFPOrFPVectorTy());
                                    // FRem is not included, because it requires fmod from libm to be linked in.
                                    Instruction::BinaryOps FpOps[] = {BinaryOperator::FAdd, BinaryOperator::FSub, BinaryOperator::FMul, BinaryOperator::FDiv};
                                    newOpCode = FpOps[rand() % (sizeof(FpOps)/sizeof(FpOps[0]))];
                                }

                                BinaryOperator* Replacement = BinaryOperator::Create(newOpCode, BO->getOperand(0), BO->getOperand(1), "", nextII);
                                BO->replaceAllUsesWith(Replacement);
                                BO->eraseFromParent();
                                
                                count_incr(fault_count_corrupt_operator_var, nextII, M);
                                val=NULL;
                                errs() << "< replaced with: ";
                                Replacement->print(errs());
                                errs() << "\n";
                                continue;
                            }
                        }

                    }
                    while(false);

                    errs() << "< ";
                    if(!val){
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

