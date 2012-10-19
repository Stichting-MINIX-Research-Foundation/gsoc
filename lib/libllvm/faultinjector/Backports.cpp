#include "Backports.h"

#define LLVM_LD_DEBUG 1

using namespace llvm;

namespace llvm {

//===----------------------------------------------------------------------===//
// Public static methods
//===----------------------------------------------------------------------===//

/// Find the debug info descriptor corresponding to this global variable.
Value *Backports::findDbgGlobalDeclare(GlobalVariable *V) {
  const Module *M = V->getParent();
  NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.gv");
  if (!NMD)
    return 0;

  for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
    DIDescriptor DIG(cast<MDNode>(NMD->getOperand(i)));
    if (!DIG.isGlobalVariable())
      continue;
    if (DIGlobalVariable(DIG).getGlobal() == V)
      return DIG;
  }
  return 0;
}

/// Find the debug info descriptor corresponding to this function.
Value *Backports::findDbgSubprogramDeclare(Function *V) {
  const Module *M = V->getParent();
  NamedMDNode *NMD = M->getNamedMetadata("llvm.dbg.sp");
  if (!NMD)
    return 0;

  for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
    DIDescriptor DIG(cast<MDNode>(NMD->getOperand(i)));
    if (!DIG.isSubprogram())
      continue;
    if (DISubprogram(DIG).getFunction() == V)
      return DIG;
  }
  return 0;
}

/// Finds the llvm.dbg.declare intrinsic corresponding to this value if any.
/// It looks through pointer casts too.
const DbgDeclareInst *Backports::findDbgDeclare(const Value *V) {
  V = V->stripPointerCasts();

  if (!isa<Instruction>(V) && !isa<Argument>(V))
    return 0;

  const Function *F = NULL;
  if (const Instruction *I = dyn_cast<Instruction>(V))
    F = I->getParent()->getParent();
  else if (const Argument *A = dyn_cast<Argument>(V))
    F = A->getParent();

  for (Function::const_iterator FI = F->begin(), FE = F->end(); FI != FE; ++FI)
    for (BasicBlock::const_iterator BI = (*FI).begin(), BE = (*FI).end();
         BI != BE; ++BI)
      if (const DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(BI))
        if (DDI->getAddress() == V)
          return DDI;

  return 0;
}

bool Backports::getLocationInfo(const Value *V, std::string &DisplayName,
                            std::string &Type, unsigned &LineNo,
                            std::string &File, std::string &Dir) {
  DICompileUnit Unit;
  DIType TypeD;

  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(const_cast<Value*>(V))) {
    Value *DIGV = findDbgGlobalDeclare(GV);
    if (!DIGV) return false;
    DIGlobalVariable Var(cast<MDNode>(DIGV));

    StringRef D = Var.getDisplayName();
    if (!D.empty())
      DisplayName = D;
    LineNo = Var.getLineNumber();
    Unit = Var.getCompileUnit();
    TypeD = Var.getType();
  } else if (Function *F = dyn_cast<Function>(const_cast<Value*>(V))){
    Value *DIF = findDbgSubprogramDeclare(F);
    if (!DIF) return false;
    DISubprogram Var(cast<MDNode>(DIF));

    StringRef D = Var.getDisplayName();
    if (!D.empty())
      DisplayName = D;
    LineNo = Var.getLineNumber();
    Unit = Var.getCompileUnit();
    TypeD = Var.getType();
  } else {
    const DbgDeclareInst *DDI = findDbgDeclare(V);
    if (!DDI) return false;
    DIVariable Var(cast<MDNode>(DDI->getVariable()));

    StringRef D = Var.getName();
    if (!D.empty())
      DisplayName = D;
    LineNo = Var.getLineNumber();
    Unit = Var.getCompileUnit();
    TypeD = Var.getType();
  }

  StringRef T = TypeD.getName();
  if (!T.empty())
    Type = T;
  StringRef F = Unit.getFilename();
  if (!F.empty())
    File = F;
  StringRef D = Unit.getDirectory();
  if (!D.empty())
    Dir = D;
  return true;
}

// llvm.dbg.region.end calls, and any globals they point to if now dead.
bool Backports::StripDebugInfo(Module &M) {

    bool Changed = false;

    // Remove all of the calls to the debugger intrinsics, and remove them from
    // the module.
    if (Function *Declare = M.getFunction("llvm.dbg.declare")) {
        while (!Declare->use_empty()) {
            CallInst *CI = cast<CallInst>(Declare->use_back());
            CI->eraseFromParent();
        }   
        Declare->eraseFromParent();
        Changed = true;
    }

    if (Function *DbgVal = M.getFunction("llvm.dbg.value")) {
        while (!DbgVal->use_empty()) {
            CallInst *CI = cast<CallInst>(DbgVal->use_back());
            CI->eraseFromParent();
        }   
        DbgVal->eraseFromParent();
        Changed = true;
    }

    for (Module::named_metadata_iterator NMI = M.named_metadata_begin(),
            NME = M.named_metadata_end(); NMI != NME;) {
        NamedMDNode *NMD = NMI;
        ++NMI;
        if (NMD->getName().startswith("llvm.dbg.")) {
            NMD->eraseFromParent();
            Changed = true;
        }   
    }

    for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI)
        for (Function::iterator FI = MI->begin(), FE = MI->end(); FI != FE; 
                ++FI)
            for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; 
                    ++BI) {
                if (!BI->getDebugLoc().isUnknown()) {
                    Changed = true;
                    BI->setDebugLoc(DebugLoc());
                }
            }   

    return Changed;
}



}
