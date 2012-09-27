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

char swapName[] = "swap";
char *SwapFault::getName(){
    return swapName;
}

bool SwapFault::isApplicable(Instruction *I){
    return dyn_cast<BinaryOperator>(I) != NULL;
}

Instruction *SwapFault::apply(Instruction *I){
    BinaryOperator *Op = dyn_cast<BinaryOperator>(I);
    /* switch operands of binary instructions */
    /* todo: if(op1.type == op2.type */
    Value *tmp = Op->getOperand(0);
    Op->setOperand(0, Op->getOperand(1));
    Op->setOperand(1, tmp);
    return I;
}

/* NoLoadFault ******************************************************************/

cl::opt<int>
NoLoadFault::prob("fault-prob-no-load",
        cl::desc("Fault Injector: load instruction loading '0' fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

char noLoadName[] = "no-load";
char *NoLoadFault::getName(){
    return noLoadName;
}

bool NoLoadFault::isApplicable(Instruction *I){
    return dyn_cast<LoadInst>(I) && dyn_cast<LoadInst>(I)->getOperand(0)->getType()->getContainedType(0)->isIntegerTy();
}

Instruction *NoLoadFault::apply(Instruction *I){
    LoadInst *LI = dyn_cast<LoadInst>(I);
    Value *newValue;
    /* load 0 instead of target value. */
    newValue = Constant::getNullValue(LI->getOperand(0)->getType()->getContainedType(0));
    LI->replaceAllUsesWith(newValue);
    LI->eraseFromParent();
    return (Instruction*) newValue;
}

/* RndLoadFault ******************************************************************/

cl::opt<int>
RndLoadFault::prob("fault-prob-rnd-load",
        cl::desc("Fault Injector: load instruction loading 'rnd()' fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

char rndLoadName[] = "rnd-load";
char *RndLoadFault::getName(){
    return rndLoadName;
}

bool RndLoadFault::isApplicable(Instruction *I){
    return dyn_cast<LoadInst>(I) && dyn_cast<LoadInst>(I)->getOperand(0)->getType()->getContainedType(0)->isIntegerTy() && I->getType()->isIntegerTy(32);
}

Instruction *RndLoadFault::apply(Instruction *I){
    Function *RandFunc = I->getParent()->getParent()->getParent()->getFunction("rand");
    assert(RandFunc);
    LoadInst *LI = dyn_cast<LoadInst>(I);
    Value *newValue = CallInst::Create(RandFunc, "", I);
    LI->replaceAllUsesWith(newValue);
    LI->eraseFromParent();
    return (Instruction*) newValue;
}

/* NoStoreFault ******************************************************************/

cl::opt<int>
NoStoreFault::prob("fault-prob-no-store",
        cl::desc("Fault Injector: remove store instruction fault probability (0 - 1000) "),
        cl::init(0), cl::NotHidden, cl::ValueRequired);

char noStoreName[] = "no-store";
char *NoStoreFault::getName(){
    return noStoreName;
}

bool NoStoreFault::isApplicable(Instruction *I){
    return dyn_cast<StoreInst>(I);
}

Instruction *NoStoreFault::apply(Instruction *I){
    StoreInst *SI = dyn_cast<StoreInst>(I);
    /* remove store instruction */
    SI->eraseFromParent();
    return NULL;
}


