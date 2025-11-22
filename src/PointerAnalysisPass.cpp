#include "PointerAnalysis.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace dataflow {

const auto PASS_NAME = "PointerAnalysis";
const auto PASS_DESC = "Standalone Pointer Analysis";

struct PointerAnalysisPass : public PassInfoMixin<PointerAnalysisPass> {
    PreservedAnalyses run(Module& M, ModuleAnalysisManager& AM) {
        outs() << "Running " << PASS_DESC << " on module " << M.getName()
               << "\n";

        for (auto& F : M) {
            if (F.isDeclaration()) continue;
            outs() << "Running " << PASS_NAME << " on " << F.getName() << "\n";
            // PointerAnalysis constructor performs the fixpoint and prints
            // results
            PointerAnalysis PA(F);
        }

        return PreservedAnalyses::all();
    }
};

// Pass registration for the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, PASS_NAME, "1.0.0", [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, ModulePassManager& MPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == PASS_NAME) {
                            MPM.addPass(PointerAnalysisPass());
                            return true;
                        }
                        return false;
                    });
            }};
}

}  // namespace dataflow
