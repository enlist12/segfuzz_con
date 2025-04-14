//==============================================================================
// FILE:
//    GetPointsPass.h
//
// DESCRIPTION:
//    collect points
//
// License: MIT
//==============================================================================

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

//------------------------------------------------------------------------------
// New PM interface
//------------------------------------------------------------------------------
struct GetPointsPass : public llvm::PassInfoMixin<GetPointsPass> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &);
public:

  llvm::Function *LogFunc;

  bool runOnModule(llvm::Module &M);

private:

  std::set<std::string> ProcessedFunctions;
  void instrumentGlobalVariableAccess(llvm::Module &M);
  void instrumentSyncOperations(llvm::Module &M);
  void instrumentMemoryOperations(llvm::Module &M);
  void instrumentStructAccesses(llvm::Module &M);
  llvm::Function *createLogFunction(llvm::Module &M);

};
