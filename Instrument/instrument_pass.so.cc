#define AFL_LLVM_PASS

#include "../include/types.h"
#include "../include/debug.h"
#include "../include/alloc.h"
#include "../include/guide_instrument.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <fstream>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;
using namespace std;

vector<string> taintResult;
vector<string> configVec;
unordered_map<string, string> hashMap;
unordered_map<string, bool> isBranchUsed;

namespace
{

  class AFLCoverage : public ModulePass
  {

  public:
    static char ID;
    AFLCoverage() : ModulePass(ID) {}

    bool runOnModule(Module &M) override;

    // StringRef getPassName() const override {
    //  return "American Fuzzy Lop Instrumentation";
    // }
  };

}

/*
  Get unique basic block identifiter consits of function name, first available (metadata info exists)
  instruction source code location , and bb name if getName() usefule

  used API: (BasicBlock & Instruction)->getParent(), BasicBlock->begin() (iterator of instructions list),
            BasicBlock->getName(), DILocation* Instruction->getDebugLoc()

  input  : tainted branch inst : Instruction* inst
  output :     uniqueID string : FunctionName_FirstNonPHILoc_BBName(if not exsit, replace it with "NoName")
*/
string getUniqueID(BasicBlock *bb) {
  Function *func = bb->getParent();
  std::string uniqueID = "";

  StringRef funcNameRef = func->getName();
  std::string funcName = funcNameRef.data();
  /// first instruction
  for (auto inst = bb->begin(); inst != bb->end(); inst++) {
    const DILocation *location = inst->getDebugLoc();
    if (location) {
      std::string directory = location->getDirectory();
      std::string filePath = location->getFilename();
      int line = location->getLine();
      std::string firstInstName = directory + "/" + filePath + ":" + to_string(line);

      if (bb->hasName()) {
        uniqueID = funcName + "_" + firstInstName + "_" + bb->getName().data();
      }
      else {
        uniqueID = funcName + "_" + firstInstName + "_" + "NoName";
      }
      return uniqueID;
    }
  }

  /// no instruction in the bb has metedata
  return "nullptr";
  /*
    // no instruction in the bb has metedata
    // llvm::outs() << "Error: Basic Block is not Normal!\n\n\n";
    // exit(0);
  */
}

/// @brief We need environment variable TAINT_RESULT to get taint result file
void getTaintResult() {
  char* fileName = getenv("TAINT_RESULT");
  if(fileName == NULL) {
    FATAL("Error!  Please set environment varible \"TAINT_RESULT\" to indicate taint analysis result!\n");
  }
  
  ifstream input(fileName);
  if(!input) {
    FATAL("Error!  Can't read taint result file!\n");
  }

  string line;
  // ofstream fout("/root/recor", ios::app);
  while(getline(input, line)) {
    if(line == "END_OF_CONFIG") {
      continue;
    }
    else {
      if(line == "\n") {
        continue;
      }
      string configName = line;
      configVec.push_back(configName);

      while(getline(input, line) && line != "END_OF_CONFIG") {
        if(hashMap.count(line) == 0) {
          hashMap.emplace(line, configName);
          isBranchUsed.emplace(line, false);
        }
        taintResult.push_back(line);
      }
    }
  }
  // fout.close();
  input.close();
}

int getConfigIndex(string& configName) {
  int n = configVec.size();
  for(int i = 0; i < n; i++) {
    if(configName == configVec[i]) {
      return i;
    }
  }

  FATAL("No config in configVec found!");
  exit(0);
}


char AFLCoverage::ID = 0;

bool AFLCoverage::runOnModule(Module &M)
{

  LLVMContext &C = M.getContext();

  IntegerType *Int8Ty = IntegerType::getInt8Ty(C);
  IntegerType *Int32Ty = IntegerType::getInt32Ty(C);

  /* Show a banner */

  char be_quiet = 0;

  if (isatty(2) && !getenv("AFL_QUIET"))
  {

    // SAYF("instrument_pass "
         // " by <ding_1597@qq.com>\n");
  }
  else
    be_quiet = 1;

  /* Decide instrumentation ratio */

  /* We don't need this env variable */
  // char* inst_ratio_str = getenv("AFL_INST_RATIO");
  unsigned int inst_ratio = 100;

  /*
  if (inst_ratio_str) {

    if (sscanf(inst_ratio_str, "%u", &inst_ratio) != 1 || !inst_ratio ||
        inst_ratio > 100)
      FATAL("Bad value of AFL_INST_RATIO (must be between 1 and 100)");

  }
  */

  /* Get globals for the SHM region and the previous location. Note that
     __afl_prev_loc is thread-local. */

  GlobalVariable *AFLMapPtr =
      new GlobalVariable(M, PointerType::get(Int32Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__afl_area_ptr");

  // GlobalVariable *AFLPrevLoc = new GlobalVariable(
  //     M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc",
  //     0, GlobalVariable::GeneralDynamicTLSModel, 0, false);


  /// set taint result vector
  getTaintResult();
  if(taintResult.size() == 0) {
    FATAL("[+] Error: Taint result vector is empty!\n");
  }

  /* Instrument all the things! */

  int inst_blocks = 0;
  for (auto &F : M)
    for (auto &BB : F)
    {
      string uniqueId = getUniqueID(&(BB));
      if(uniqueId == "nullptr" || hashMap.count(uniqueId) == 0 || isBranchUsed[uniqueId]) {
        continue;
      }

      isBranchUsed[uniqueId] = true;
      string configName = hashMap[uniqueId];
      // __afl_area_ptr[0] is a flag value, we should fill the config bitmap from index 1
      unsigned int index = getConfigIndex(configName) + 1;

      BasicBlock::iterator IP = BB.getFirstInsertionPt();
      IRBuilder<> IRB(&(*IP));

      if (AFL_R(100) >= inst_ratio)
        continue;

      /* Make up cur_loc */

      // unsigned int cur_loc = AFL_R(SHM_MAP_SIZE);

      // ConstantInt *CurLoc = ConstantInt::get(Int32Ty, cur_loc);
      ConstantInt *CurLoc = ConstantInt::get(Int32Ty, index);
      /* 
        /// Load prev_loc

        LoadInst *PrevLoc = IRB.CreateLoad(AFLPrevLoc);
        PrevLoc->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
        Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt32Ty());
      */

      /* Load SHM pointer */

      LoadInst *MapPtr = IRB.CreateLoad(AFLMapPtr);
      MapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      // Value *MapPtrIdx =
      //     IRB.CreateGEP(MapPtr, IRB.CreateXor(PrevLocCasted, CurLoc));
      Value *MapPtrIdx =
           IRB.CreateGEP(MapPtr, CurLoc);

      /* Update bitmap */

      LoadInst *Counter = IRB.CreateLoad(MapPtrIdx);
      Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      Value *Incr = IRB.CreateAdd(Counter, ConstantInt::get(Int32Ty, 1));
      IRB.CreateStore(Incr, MapPtrIdx)
          ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

      /* 
      /// Set prev_loc to cur_loc >> 1 

      StoreInst *Store =
          IRB.CreateStore(ConstantInt::get(Int32Ty, cur_loc >> 1), AFLPrevLoc);
      Store->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      */
      inst_blocks++;
    }
   
  /* Say something nice. */
  if (!be_quiet)
  {

    if (!inst_blocks) {
     // WARNF("No instrumentation targets found in the file.");
     }
    else{
      SAYF("We get %d locations to instrument!\n", int(taintResult.size()));
      SAYF("We get %d hashmap elements!\n", int(hashMap.size()));
      SAYF("Instrumented %u locations (%s mode, ratio %u%%).\n",
           inst_blocks, getenv("AFL_HARDEN") ? "hardened" : ((getenv("AFL_USE_ASAN") || getenv("AFL_USE_MSAN")) ? "ASAN/MSAN" : "non-hardened"), inst_ratio);
    }
  }

  return true;
}

static void registerAFLPass(const PassManagerBuilder &,
                            legacy::PassManagerBase &PM)
{

  PM.add(new AFLCoverage());
}

static RegisterStandardPasses RegisterAFLPass(
    PassManagerBuilder::EP_OptimizerLast, registerAFLPass);

static RegisterStandardPasses RegisterAFLPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerAFLPass);
