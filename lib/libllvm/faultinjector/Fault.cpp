#include "Fault.h"

/* FaultType ******************************************************************/

void FaultType::addToModule(Module &M){
    ConstantInt* Constant0 = ConstantInt::get(M.getContext(), APInt(32, StringRef("0"), 10));

    Function* fault_print_stat = M.getFunction("fault_print_stat");
    assert(fault_print_stat && "function fault_print_stat() not found");

    Function* fault_print_stats = M.getFunction("fault_print_stats");
    assert(fault_print_stats && "function fault_print_stats() not found");

    Instruction *endOfFunc = &(*fault_print_stats->getEntryBlock().getTerminator());

    GlobalVariable* fault_count = new GlobalVariable(/*Module=*/M, 
            /*Type=*/IntegerType::get(M.getContext(), 32),
            /*isConstant=*/false,
            /*Linkage=*/GlobalValue::ExternalLinkage,
            /*Initializer=*/0, // has initializer, specified below
            /*Name=*/Twine("fault_count_", StringRef(this->getName())));
    fault_count->setAlignment(4);
    fault_count->setInitializer(Constant0);

    this->fault_count = fault_count;

    GlobalVariable* fault_name = new GlobalVariable(/*Module=*/M, 
            /*Type=*/ArrayType::get(IntegerType::get(M.getContext(), 8), strlen(this->getName())+1),
            /*isConstant=*/false,
            /*Linkage=*/GlobalValue::ExternalLinkage,
            /*Initializer=*/0, // has initializer, specified below
            /*Name=*/Twine("fault_name_", StringRef(this->getName())));
    fault_name->setAlignment(1);
    fault_name->setInitializer(ConstantArray::get(M.getContext(), this->getName(), true));

    std::vector<Constant*> fault_name_ptr_indices;
    fault_name_ptr_indices.push_back(Constant0);
    fault_name_ptr_indices.push_back(Constant0);
    Constant* fault_name_ptr = ConstantExpr::getGetElementPtr(fault_name, &fault_name_ptr_indices[0], fault_name_ptr_indices.size()); 

    LoadInst* load_fault_count = new LoadInst(fault_count, "", false, endOfFunc);
    load_fault_count->setAlignment(4);

    std::vector<Value*> fault_print_stat_params;
    fault_print_stat_params.push_back(fault_name_ptr);
    fault_print_stat_params.push_back(load_fault_count);

    CallInst* fault_print_stat_call = CallInst::Create(fault_print_stat, fault_print_stat_params.begin(), fault_print_stat_params.end(), "", endOfFunc);
    fault_print_stat_call->setCallingConv(CallingConv::C);
    fault_print_stat_call->setTailCall(false);
    AttrListPtr fault_print_stat_call_PAL;
    fault_print_stat_call->setAttributes(fault_print_stat_call_PAL);
}

/* SwapFault ******************************************************************/

cl::opt<int>
SwapFault::prob("fault-prob-swap",
        cl::desc("Fault Injector: binary operand swap fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *SwapFault::getName(){
    return "swap";
}

bool SwapFault::isApplicable(Instruction *I){
    return dyn_cast<BinaryOperator>(I) != NULL;
}

bool SwapFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    BinaryOperator *Op = dyn_cast<BinaryOperator>(I);
    /* switch operands of binary instructions */
    /* todo: if(op1.type == op2.type */
    Value *tmp = Op->getOperand(0);
    Op->setOperand(0, Op->getOperand(1));
    Op->setOperand(1, tmp);
    replacements->push_back(I);
    return true;
}

/* NoLoadFault ******************************************************************/

cl::opt<int>
NoLoadFault::prob("fault-prob-no-load",
        cl::desc("Fault Injector: load instruction loading '0' fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *NoLoadFault::getName(){
    return "no-load";
}

bool NoLoadFault::isApplicable(Instruction *I){
    return dyn_cast<LoadInst>(I) && dyn_cast<LoadInst>(I)->getOperand(0)->getType()->getContainedType(0)->isIntegerTy();
}

bool NoLoadFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    LoadInst *LI = dyn_cast<LoadInst>(I);
    Value *newValue;
    /* load 0 instead of target value. */
    newValue = Constant::getNullValue(LI->getOperand(0)->getType()->getContainedType(0));
    LI->replaceAllUsesWith(newValue);
    LI->eraseFromParent();
    replacements->push_back(newValue);
    return true;
}

/* RndLoadFault ******************************************************************/

cl::opt<int>
RndLoadFault::prob("fault-prob-rnd-load",
        cl::desc("Fault Injector: load instruction loading 'rnd()' fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *RndLoadFault::getName(){
    return "rnd-load";
}

bool RndLoadFault::isApplicable(Instruction *I){
    return dyn_cast<LoadInst>(I) && dyn_cast<LoadInst>(I)->getOperand(0)->getType()->getContainedType(0)->isIntegerTy() && I->getType()->isIntegerTy(32);
}

bool RndLoadFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    Function *RandFunc = I->getParent()->getParent()->getParent()->getFunction("rand");
    assert(RandFunc);
    LoadInst *LI = dyn_cast<LoadInst>(I);
    Value *newValue = CallInst::Create(RandFunc, "", I);
    LI->replaceAllUsesWith(newValue);
    LI->eraseFromParent();
    replacements->push_back(newValue);
    return true;
}

/* NoStoreFault ******************************************************************/

cl::opt<int>
NoStoreFault::prob("fault-prob-no-store",
        cl::desc("Fault Injector: remove store instruction fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *NoStoreFault::getName(){
    return "no-store";
}

bool NoStoreFault::isApplicable(Instruction *I){
    return dyn_cast<StoreInst>(I);
}

bool NoStoreFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    StoreInst *SI = dyn_cast<StoreInst>(I);
    /* remove store instruction */
    SI->eraseFromParent();
    return true;
}

/* FlipBranchFault ******************************************************************/

cl::opt<int>
FlipBranchFault::prob("fault-prob-flip-branch",
        cl::desc("Fault Injector: flip branch fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *FlipBranchFault::getName(){
    return "flip-branch";
}

bool FlipBranchFault::isApplicable(Instruction *I){
    if(I->getType()->isIntegerTy(1)){
        for(Value::use_iterator U = I->use_begin(), UE = I->use_end(); U != UE; U++){
            if (dyn_cast<BranchInst>(*U)) {
                return true;
            }
        }
    }
    return false;

}

bool FlipBranchFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
                            
    /* boolean value is used as branching condition */
    for(Value::use_iterator U = I->use_begin(), UE = I->use_end(); U != UE; U++){
        BranchInst *BI;
        if ((BI = dyn_cast<BranchInst>(*U))) {
            //errs() << "Negated branch instruction\n";
            ConstantInt* True = ConstantInt::get(I->getParent()->getParent()->getParent()->getContext(), APInt(1, StringRef("-1"), 10));
            BinaryOperator* Negation = BinaryOperator::Create(Instruction::Xor, I, True, "", BI);
            BI->setOperand(0, Negation);
            replacements->push_back(Negation);
            return false;
        }
    }
    assert(0 && "should not be reached");
    return false;
    }

/* FlipBoolFault ******************************************************************/

cl::opt<int>
FlipBoolFault::prob("fault-prob-flip-bool",
        cl::desc("Fault Injector: flip boolean value fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *FlipBoolFault::getName(){
    return "flip-bool";
}

bool FlipBoolFault::isApplicable(Instruction *I){
    return I->getType()->isIntegerTy(1);
}

bool FlipBoolFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    /* boolean value, not necessarily used as branching condition */

    ConstantInt* True = ConstantInt::get(I->getParent()->getParent()->getParent()->getContext(), APInt(1, StringRef("-1"), 10));
    BinaryOperator* Negation = BinaryOperator::Create(Instruction::Xor, I, True, "", I->getNextNode());
    I->replaceAllUsesWith(Negation);
    Negation->setOperand(0, I); /* restore the use in the negation instruction */
    //errs() << "< inserted (after next): ";
    //Negation->print(errs());
    //errs() << "\n";
    replacements->push_back(Negation);
    return false;
}

/* CorruptPointerFault ******************************************************************/

cl::opt<int>
CorruptPointerFault::prob("fault-prob-corrupt-pointer",
        cl::desc("Fault Injector: corrupt pointer fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *CorruptPointerFault::getName(){
    return "corrupt-pointer";
}

bool CorruptPointerFault::isApplicable(Instruction *I){
    return I->getType()->isPointerTy();
}

bool CorruptPointerFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    Instruction *next = I->getNextNode();
    Module *M = I->getParent()->getParent()->getParent();
    Function *RandFunc = M->getFunction("rand");
    CallInst* PtrRandFuncCall = CallInst::Create(RandFunc, "", next);
    CastInst* rand64 = new SExtInst(PtrRandFuncCall, IntegerType::get(M->getContext(), 64), "", next);
    CastInst* rnd_ptr = new IntToPtrInst(rand64, I->getType(), "", next);    
    I->replaceAllUsesWith(rnd_ptr);
    I->eraseFromParent();

    replacements->push_back(PtrRandFuncCall);
    replacements->push_back(rand64);
    replacements->push_back(rnd_ptr);
    return true;
}

/* CorruptIndexFault ******************************************************************/

cl::opt<int>
CorruptIndexFault::prob("fault-prob-corrupt-index",
        cl::desc("Fault Injector: corrupt index fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *CorruptIndexFault::getName(){
    return "corrupt-index";
}

bool CorruptIndexFault::isApplicable(Instruction *I){
    if(I->getType()->isIntegerTy(32)){
        for(Value::use_iterator U = I->use_begin(), UE = I->use_end(); U != UE; U++){
            if (dyn_cast<GetElementPtrInst>(*U)) {
                return true;
            }
        }
    }
    return false;
}

bool CorruptIndexFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    if(I->getType()->isIntegerTy(32)){
        for(Value::use_iterator U = I->use_begin(), UE = I->use_end(); U != UE; U++){
            if (dyn_cast<GetElementPtrInst>(*U)) {
                Instruction::BinaryOps opcode;
                if(rand() % 2){
                    opcode=Instruction::Add;
                }else{
                    opcode=Instruction::Sub;
                }
                Module *M = I->getParent()->getParent()->getParent();
                ConstantInt* Constant1 = ConstantInt::get(M->getContext(), APInt(32, StringRef("1"), 10));
                BinaryOperator* Change = BinaryOperator::Create(opcode, I, Constant1, "", I->getNextNode());
                I->replaceAllUsesWith(Change);
                Change->setOperand(0, I); /* restore the use in the change instruction */
                replacements->push_back(Change);
                return false;
            }
        }
    }
    assert(0 && "should not be reached");
    return false;
}

/* CorruptIntegerFault ******************************************************************/

cl::opt<int>
CorruptIntegerFault::prob("fault-prob-corrupt-integer",
        cl::desc("Fault Injector: corrupt integer fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *CorruptIntegerFault::getName(){
    return "corrupt-integer";
}

bool CorruptIntegerFault::isApplicable(Instruction *I){
    return I->getType()->isIntegerTy(32);
}

bool CorruptIntegerFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    Instruction::BinaryOps opcode;
    if(rand() % 2){
        opcode=Instruction::Add;
    }else{
        opcode=Instruction::Sub;
    }
    Module *M = I->getParent()->getParent()->getParent();
    ConstantInt* Constant1 = ConstantInt::get(M->getContext(), APInt(32, StringRef("1"), 10));
    BinaryOperator* Change = BinaryOperator::Create(opcode, I, Constant1, "", I->getNextNode());
    I->replaceAllUsesWith(Change);
    Change->setOperand(0, I); /* restore the use in the change instruction */
    replacements->push_back(Change);
    return false;
}

/* CorruptOperatorFault ******************************************************************/

cl::opt<int>
CorruptOperatorFault::prob("fault-prob-corrupt-operator",
        cl::desc("Fault Injector: corrupt binary operator fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

const char *CorruptOperatorFault::getName(){
    return "corrupt-operator";
}

bool CorruptOperatorFault::isApplicable(Instruction *I){
    return dyn_cast<BinaryOperator>(I);
}

bool CorruptOperatorFault::apply(Instruction *I, SmallVectorImpl<Value *> *replacements){
    BinaryOperator *BO = dyn_cast<BinaryOperator>(I);

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

    BinaryOperator* Replacement = BinaryOperator::Create(newOpCode, BO->getOperand(0), BO->getOperand(1), "", I->getNextNode());
    BO->replaceAllUsesWith(Replacement);
    BO->eraseFromParent();

    //errs() << "< replaced with: ";
    //Replacement->print(errs());
    //errs() << "\n";
    replacements->push_back(Replacement);
    return true;
}

