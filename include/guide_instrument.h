#ifndef GUIDED_INSTRUMENT_H
#define GUIDED_INSTRUMENT_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
// Ding adds head file DebugLoc.h
#include "llvm/IR/DebugLoc.h"
// Ding adds head file SymbolTableListTraits.h
#include "llvm/IR/SymbolTableListTraits.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm-c/IRReader.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/ADT/BreadthFirstIterator.h"
// Ding adds head file StringRef.h
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"



#include <fstream>
#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <list>
#include <string>
#include <sstream>
#include <ctime>

#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>


// #include <stdlib.h>
#include <cxxabi.h>
#include <typeinfo> //for 'typeid' to work  

using namespace std;
using namespace llvm;

 

string getUniqueID(BasicBlock* bb);
void getTaintResult();
int getConfigIndex(string& configName);

#endif