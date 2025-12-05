#include "UseAfterFreeAnalysis.h"

#include "Utils.h"
#include <llvm/Passes/PassPlugin.h>

namespace dataflow {

//===----------------------------------------------------------------------===//
// UseAfterFree Analysis Implementation
//===----------------------------------------------------------------------===//

bool UseAfterFreeAnalysis::check(Instruction* Inst) {
  /**
   * Inst can cause a use-after-free if:
   *   Inst load, store, or call instruction,
   *   The operand is either Freed or MaybeFreed.
   */
  Memory* In = InMap[Inst];

  // Check for loading a freed pointer
  if (auto* Load = dyn_cast<LoadInst>(Inst)) {
    Value* Ptr = Load->getPointerOperand();
    Domain* D = getOrExtract(In, Ptr);

    return (D->Value == Domain::Freed || D->Value == Domain::MaybeFreed);
  }

  // Check for storing a value to a freed pointer
  if (auto* Store = dyn_cast<StoreInst>(Inst)) {
    Value* Ptr = Store->getPointerOperand();
    Domain* D = getOrExtract(In, Ptr);

    return (D->Value == Domain::Freed || D->Value == Domain::MaybeFreed);
  }

  // Check for call instruction with freed pointer argument
  if (auto* Call = dyn_cast<CallInst>(Inst)) {
    for (unsigned i = 0; i < Call->arg_size(); ++i) {
      Value* Arg = Call->getArgOperand(i);

      if (!Arg->getType()->isPointerTy()) {
        continue;
      }

      Domain* D = getOrExtract(In, Arg);

      if (D->Value == Domain::Freed || D->Value == Domain::MaybeFreed) {
        return true;
      }
    }
  }

  return false;
}

const auto PASS_NAME = "UseAfterFree";
const auto PASS_DESC = "Use-after-free Analysis";

PreservedAnalyses UseAfterFreeAnalysis::run(Module& M, ModuleAnalysisManager& AM) {
  outs() << "Running " << PASS_DESC << " on module " << M.getName() << "\n";

  for (auto& F : M) {
    if (F.isDeclaration()) {
      continue;
    }

    outs() << "Running " << getAnalysisName() << " on " << F.getName() << "\n";

    ErrorInsts.clear();

    // Initializing InMap and OutMap.
    for (inst_iterator Iter = inst_begin(F), End = inst_end(F); Iter != End; ++Iter) {
      auto Inst = &(*Iter);
      InMap[Inst] = new Memory;
      OutMap[Inst] = new Memory;
    }

    // The chaotic iteration algorithm is implemented inside doAnalysis().
    auto PA = new DoubleFreePointerAnalysis(F);
    doAnalysis(F, PA);

    // Check each instruction in function F for potential divide-by-zero error.
    for (inst_iterator Iter = inst_begin(F), End = inst_end(F); Iter != End; ++Iter) {
      auto Inst = &(*Iter);
      if (check(Inst))
        ErrorInsts.insert(Inst);
    }

    printMap(F, InMap, OutMap);
    outs() << "Potential Instructions by " << getAnalysisName() << ": \n";
    for (auto Inst : ErrorInsts) {
      outs() << *Inst << "\n";
    }

    for (auto Iter = inst_begin(F), End = inst_end(F); Iter != End; ++Iter) {
      delete InMap[&(*Iter)];
      delete OutMap[&(*Iter)];
    }
  }

  return PreservedAnalyses::all();
}

// Pass registration for the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, PASS_NAME, "1.0.0", [](PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                    ModulePassManager& MPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == PASS_NAME) {
                    MPM.addPass(UseAfterFreeAnalysis());
                    return true;
                  }
                  return false;
                });
          }};
}

}  // namespace dataflow
