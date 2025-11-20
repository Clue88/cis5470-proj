#include "DoubleFreeAnalysis.h"

#include "Utils.h"
#include <llvm/Passes/PassPlugin.h>

namespace dataflow {

//===----------------------------------------------------------------------===//
// DoubleFree Analysis Implementation
//===----------------------------------------------------------------------===//

bool DoubleFreeAnalysis::check(Instruction* Inst) {
  /**
   * Inst can cause a double-free if:
   *   Inst call instruction with name `free`,
   *   The operand is either Freed or MaybeFreed.
   */
  auto* Call = dyn_cast<CallInst>(Inst);
  if (!Call) {
    return false;
  }

  Function* Callee = Call->getCalledFunction();
  if (!Callee) {
    return false;
  }

  if (!Callee->getName().equals("free")) {
    return false;
  }

  if (Call->arg_size() < 1) {
    return false;  // shouldn't be possible, since free takes 1 arg
  }

  Value* Ptr = Call->getArgOperand(0);
  Memory* In = InMap[Inst];

  // Track the freed-ness of the pointer value
  Domain* D = getOrExtract(In, Ptr);

  return (D->Value == Domain::Freed || D->Value == Domain::MaybeFreed);
}

const auto PASS_NAME = "DoubleFree";
const auto PASS_DESC = "Double-free Analysis";

PreservedAnalyses DoubleFreeAnalysis::run(Module& M, ModuleAnalysisManager& AM) {
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
    auto PA = new PointerAnalysis(F);
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
                    MPM.addPass(DoubleFreeAnalysis());
                    return true;
                  }
                  return false;
                });
          }};
}

}  // namespace dataflow
