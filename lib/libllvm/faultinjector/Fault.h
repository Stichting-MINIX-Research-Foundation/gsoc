using namespace llvm;

namespace llvm {

class FaultInjector : public ModulePass {

  public:
      static char ID;

      FaultInjector();

      virtual bool runOnModule(Module &M);

};

class FaultType{
public:
    int i;
};

class SwapFault : public FaultType {
public:
    int j;
    void printTest();
};

}

