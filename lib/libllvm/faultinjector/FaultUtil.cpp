#include "FaultUtil.h"

// From http://www.infernodevelopment.com/perfect-c-string-explode-split

using namespace llvm;

namespace llvm{

    void StringExplode(std::string str, std::string separator, std::vector<std::string>* results){
        unsigned found;
        found = str.find_first_of(separator);
        while(found != std::string::npos){
            if(found > 0){
                results->push_back(str.substr(0,found));
            }
            str = str.substr(found+1);
            found = str.find_first_of(separator);
        }
        if(str.length() > 0){
            results->push_back(str);
        }
    }

    void count_incr(GlobalVariable *counter, Instruction *insertBefore, Module &M){
        
        ConstantInt* Constant1 = ConstantInt::get(M.getContext(), APInt(32, StringRef("1"), 10));

        LoadInst* count_var = new LoadInst(counter, "", false, insertBefore);
        count_var->setAlignment(4);
        BinaryOperator* count_incr = BinaryOperator::Create(Instruction::Add, count_var, Constant1, "", insertBefore);
        StoreInst *store_count_var = new StoreInst(count_incr, counter, false, insertBefore);
        store_count_var->setAlignment(4);
    }
}
