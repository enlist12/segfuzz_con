//===- GetPointsPass.cpp - Linux Kernel Instrumentation Pass --------------===//
//
// This file implements an LLVM pass for instrumenting the Linux kernel to
// collect information about global variables, lock operations, semaphore
// operations, memory allocation/deallocation/copy operations, and memory
// accesses to struct objects.
//
// The pass records memory addresses of these operations and writes them to a file
// for further analysis.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "GetPointsPass.h"
#include <set>
#include <string>

using namespace llvm;

#define DEBUG_TYPE "get-points"
 
  
void GetPointsPass::instrumentSyncOperations(Module &M) {
    
    
  std::vector<std::string> SyncFuncs = {
      "mutex_lock", "mutex_unlock", "mutex_trylock",
      "spin_lock", "spin_unlock", "spin_trylock",
      "raw_spin_lock", "raw_spin_unlock", "raw_spin_trylock",
      "_raw_spin_lock", "_raw_spin_unlock", "_raw_spin_trylock",
      "read_lock", "read_unlock", "write_lock", "write_unlock",
      "down_read", "up_read", "down_write", "up_write",
      "_raw_read_lock", "_raw_read_unlock", "_raw_write_lock", "_raw_write_unlock",
      "down", "up", "down_interruptible",
      "down_timeout", "down_trylock", "down_killable",
      "__down", "__up", "__down_interruptible",
      "__down_timeout", "__down_trylock", "__down_killable",
      "down_read_trylock", "down_write_trylock"
    };
    
    
  for (auto &F : M) {
    std::string FName = F.getName().str();
      
    bool isSyncFunc = false;
    for (const auto &SyncFunc : SyncFuncs) {
      if (FName == SyncFunc || FName.find(SyncFunc) != std::string::npos || FName.find("lock") != std::string::npos) {
        isSyncFunc = true;
        break;
      }
    }
      
    if (isSyncFunc) {
      for (auto *U : F.users()) {
        if (auto *Call = dyn_cast<CallInst>(U)) {
          IRBuilder<> Builder(Call);
          Builder.CreateCall(LogFunc);
        }
      }
    }
  }
}
  

Function *GetPointsPass::createLogFunction(Module &M) {
  LLVMContext &Context = M.getContext();
  FunctionCallee collect=
      M.getOrInsertFunction("collect_info",FunctionType::get(Type::getVoidTy(Context),
                            Type::getVoidTy(Context), false));
  Function *LogFunc = dyn_cast<Function>(collect.getCallee());
  LogFunc->setDoesNotThrow();
  return LogFunc;
}

void GetPointsPass::instrumentGlobalVariableAccess(Module &M) {
  
  for (auto &GV : M.globals()) {

    if (GV.isConstant() || GV.getAddressSpace() != 0)
      continue;
      
    std::string Name = GV.getName().str();
    if (!Name.empty()) {
      for (auto *U : GV.users()) {
        if (auto *I = dyn_cast<Instruction>(U)) {
          IRBuilder<> Builder(I);
          Builder.CreateCall(LogFunc);
        }
      }
    }
  }
}


void GetPointsPass::instrumentMemoryOperations(Module &M) {

  std::vector<std::string> MemFuncs = {
    "kmalloc", "kzalloc", "kmem_cache_alloc", "kmem_cache_zalloc",
    "__kmalloc", "krealloc", "kfree", "kmem_cache_free",
    "memcpy", "memmove", "memset", "copy_from_user", "copy_to_user",
    "__copy_from_user", "__copy_to_user"
  };

  for (auto &F : M) {
    std::string FName = F.getName().str();

    bool isMemFunc = false;
    for (const auto &MemFunc : MemFuncs) {
      if (FName == MemFunc || FName.find(MemFunc) != std::string::npos) {
        isMemFunc = true;
        break;
      }
    }
    
    if (isMemFunc) {

      for (auto *U : F.users()) {
        if (auto *Call = dyn_cast<CallInst>(U)) {
          IRBuilder<> Builder(Call);
          Builder.CreateCall(LogFunc);
        }
      }
    }
  }
}


void GetPointsPass::instrumentStructAccesses(Module &M) {

  for (auto &F : M) {
    if (ProcessedFunctions.count(F.getName().str()))
      continue;

    for (auto &BB : F) {
      for (auto &I : BB) {

        if (auto *LI = dyn_cast<LoadInst>(&I)) {

          Value *Ptr = LI->getPointerOperand();
          if (auto *PT = dyn_cast<GetElementPtrInst>(Ptr)) {
            Type *ElTy = PT->getOperand(0)->getType();
            if (ElTy->isStructTy()) {
              IRBuilder<> Builder(LI);
              Builder.CreateCall(LogFunc);
            }
          }
        }
        else if (auto *SI = dyn_cast<StoreInst>(&I)) {
          Value *Ptr = SI->getPointerOperand();
          if (auto *PT = dyn_cast<GetElementPtrInst>(Ptr)) {
            Type *ElTy = PT->getOperand(0)->getType();
            if (ElTy->isStructTy()) {
              IRBuilder<> Builder(SI);
              Builder.CreateCall(LogFunc);
            }
          }
        }
      }
    }

    ProcessedFunctions.insert(F.getName().str());
  }
}

bool GetPointsPass::runOnModule(Module &M) {
  LogFunc = createLogFunction(M);

  instrumentGlobalVariableAccess(M);
  instrumentSyncOperations(M);
  instrumentMemoryOperations(M);
  instrumentStructAccesses(M);
  
  return true;
}

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
      PB.registerPipelineEarlySimplificationEPCallback([&](ModulePassManager &MPM,OptimizationLevel OL, ThinOrFullLTOPhase) {
          MPM.addPass(GetPointsPass());
          return true;
      });
  };

  return {LLVM_PLUGIN_API_VERSION, "GetPointsPass", "0.0.1", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}