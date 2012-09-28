#include "llvm/Pass.h"
#include <llvm/Instructions.h>
#include "llvm/Support/CommandLine.h"
#include <llvm/Module.h>

using namespace llvm;

class FaultType{
private:
    GlobalVariable *fault_count;
public:
    virtual bool isApplicable(Instruction *I) = 0;
    virtual Instruction *apply(Instruction *I) = 0;
    virtual const char *getName() = 0;
    virtual int getProbability() = 0;

    void addToModule(Module &M);
    GlobalVariable *getFaultCount(){
        return fault_count;
    }

};

#define FAULT_MEMBERS \
    bool isApplicable(Instruction *I); \
    Instruction *apply(Instruction *I); \
    const char *getName();\
    static cl::opt<int> prob;\
    int getProbability(){\
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

