#include "llvm/Pass.h"
#include <llvm/Instructions.h>
#include "llvm/Support/CommandLine.h"
#include <llvm/Module.h>

using namespace llvm;


extern cl::opt<int> prob_default;

class FaultType{
private:
    GlobalVariable *fault_count;
public:
    virtual bool isApplicable(Instruction *I) = 0;
    virtual bool apply(Instruction *I, SmallVectorImpl<Value *> *replacements) = 0;
    virtual const char *getName() = 0;
    virtual int getProbability() = 0;

    void addToModule(Module &M);

    GlobalVariable *getFaultCount(){
        return fault_count;
    }
};

#define FAULT_MEMBERS \
    bool isApplicable(Instruction *I); \
    bool apply(Instruction *I, SmallVectorImpl<Value *> *replacements); \
    const char *getName();\
    static cl::opt<int> prob;\
    int getProbability(){\
        if(prob.getNumOccurrences() == 0 && prob_default.getNumOccurrences() >> 0){\
            return prob_default;\
        }\
        return prob;\
    }

class SwapFault : public FaultType {
    public: FAULT_MEMBERS
};

class NoLoadFault : public FaultType {
    public: FAULT_MEMBERS
};

class RndLoadFault : public FaultType {
    public: FAULT_MEMBERS
};

class NoStoreFault : public FaultType {
    public: FAULT_MEMBERS
};

class FlipBranchFault : public FaultType {
    public: FAULT_MEMBERS
};

class FlipBoolFault : public FaultType {
    public: FAULT_MEMBERS
};

class CorruptPointerFault : public FaultType {
    public: FAULT_MEMBERS
};

class CorruptIndexFault : public FaultType {
    public: FAULT_MEMBERS
};

class CorruptIntegerFault : public FaultType {
    public: FAULT_MEMBERS
};

class CorruptOperatorFault : public FaultType {
    public: FAULT_MEMBERS
};

