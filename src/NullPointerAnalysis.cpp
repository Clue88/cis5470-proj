#include "PointerAnalysis.h"

#include "Domain.h"
#include "Utils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

namespace dataflow {

void PointerAnalysis::transfer(Instruction* Inst, PointsToInfo& PointsTo) {
    if (AllocaInst* Alloca = dyn_cast<AllocaInst>(Inst)) {
        PointsToSet& S = PointsTo[variable(Alloca)];
        S.insert(address(Alloca));
        // update null state for the allocated variable
        auto NewState = computeNullState(variable(Alloca), PointsTo);
        if (NullStates[variable(Alloca)] != NewState) {
            NullStates[variable(Alloca)] = NewState;
            NullChanged = true;
        }

    } else if (StoreInst* Store = dyn_cast<StoreInst>(Inst)) {
        Value* Pointer = Store->getPointerOperand();
        Value* ValueOp = Store->getValueOperand();

        // If the RHS is not a pointer, nothing to update
        if (!ValueOp->getType()->isPointerTy()) return;

        // RHS could be an explicit null constant or another pointer variable
        PointsToSet R;
        if (isa<ConstantPointerNull>(ValueOp)) {
            R.insert(std::string("NULL"));
        } else {
            R = PointsTo[variable(ValueOp)];
        }

        // Store writes RHS into memory location(s) pointed to by Pointer
        PointsToSet& L = PointsTo[variable(Pointer)];
        for (auto& MemLoc : L) {
            PointsToSet& S = PointsTo[MemLoc];
            // Union the RHS into what's stored at this memory location
            PointsToSet Result;
            std::set_union(S.begin(), S.end(), R.begin(), R.end(),
                           std::inserter(Result, Result.begin()));
            if (PointsTo[MemLoc] != Result) {
                PointsTo[MemLoc] = Result;
                // recompute null state for this memory location
                auto NewState = computeNullState(MemLoc, PointsTo);
                if (NullStates[MemLoc] != NewState) {
                    NullStates[MemLoc] = NewState;
                    NullChanged = true;
                }
            }
        }

    } else if (LoadInst* Load = dyn_cast<LoadInst>(Inst)) {
        // Always check loads (they dereference a pointer operand) — do not
        // gate on the load result type (the loaded value may be non-pointer).
        // Check whether we are dereferencing a pointer that may point-to NULL
        Value* Ptr = Load->getPointerOperand();
        // set NullState for the value produced by the load
        auto NewState = computeNullState(variable(Load), PointsTo);
        if (NullStates[variable(Load)] != NewState) {
            NullStates[variable(Load)] = NewState;
            NullChanged = true;
        }

        std::string VariableName = variable(Load->getPointerOperand());
        PointsToSet& R = PointsTo[VariableName];
        PointsToSet Result;
        for (auto& I : R) {
            PointsToSet& S = PointsTo[I];
            std::set_union(S.begin(), S.end(), Result.begin(), Result.end(),
                           std::inserter(Result, Result.begin()));
        }
        PointsTo[variable(Load)] = Result;

    } else if (auto* Call = dyn_cast<CallInst>(Inst)) {
        if (Call->getType()->isPointerTy()) {
            PointsToSet& S = PointsTo[variable(Call)];
            S.insert(address(Call));
            auto NewState = computeNullState(variable(Call), PointsTo);
            if (NullStates[variable(Call)] != NewState) {
                NullStates[variable(Call)] = NewState;
                NullChanged = true;
            }
        }

    } else if (auto* Cast = dyn_cast<CastInst>(Inst)) {
        if (Cast->getType()->isPointerTy() &&
            Cast->getOperand(0)->getType()->isPointerTy()) {
            PointsTo[variable(Cast)] = PointsTo[variable(Cast->getOperand(0))];
            auto NewState = computeNullState(variable(Cast), PointsTo);
            if (NullStates[variable(Cast)] != NewState) {
                NullStates[variable(Cast)] = NewState;
                NullChanged = true;
            }
        }

    } else if (auto* GEP = dyn_cast<GetElementPtrInst>(Inst)) {
        if (GEP->getType()->isPointerTy()) {
            PointsTo[variable(GEP)] =
                PointsTo[variable(GEP->getPointerOperand())];
            auto NewState = computeNullState(variable(GEP), PointsTo);
            if (NullStates[variable(GEP)] != NewState) {
                NullStates[variable(GEP)] = NewState;
                NullChanged = true;
            }
        }

    } else if (auto* Phi = dyn_cast<PHINode>(Inst)) {
        if (!Phi->getType()->isPointerTy()) {
            return;
        }

        PointsToSet Result;
        for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
            Value* Incoming = Phi->getIncomingValue(i);
            if (!Incoming->getType()->isPointerTy()) {
                continue;
            }
            PointsToSet& S = PointsTo[variable(Incoming)];
            std::set_union(S.begin(), S.end(), Result.begin(), Result.end(),
                           std::inserter(Result, Result.begin()));
        }
        PointsTo[variable(Phi)] = Result;
        auto NewState = computeNullState(variable(Phi), PointsTo);
        if (NullStates[variable(Phi)] != NewState) {
            NullStates[variable(Phi)] = NewState;
            NullChanged = true;
        }
    }
}

// Helper: recursively check whether variable `var` may (transitively) point to
// NULL
static bool mayPointToNull(const std::string& var, PointsToInfo& PointsTo,
                           int depth = 3) {
    if (depth <= 0) return false;
    auto it = PointsTo.find(var);
    if (it == PointsTo.end()) return false;
    const PointsToSet& pts = it->second;
    for (const auto& t : pts) {
        if (t == "NULL") return true;
        // if we have a PointsTo entry for the target, recurse
        auto it2 = PointsTo.find(t);
        if (it2 != PointsTo.end()) {
            if (mayPointToNull(t, PointsTo, depth - 1)) return true;
        }
    }
    return false;
}

int PointerAnalysis::countFacts(PointsToInfo& PointsTo) {
    int N = 0;
    for (auto& I : PointsTo) N += I.second.size();
    return N;
}

Domain::NullState PointerAnalysis::computeNullState(const std::string& var,
                                                    PointsToInfo& PointsTo) {
    auto it = PointsTo.find(var);
    if (it == PointsTo.end()) return dataflow::Domain::Unknown;
    const PointsToSet& pts = it->second;
    bool hasNull = pts.find(std::string("NULL")) != pts.end();
    bool hasAddr = false;
    for (auto& t : pts) {
        if (t != "NULL") {
            hasAddr = true;
            break;
        }
    }
    if (hasNull && !hasAddr) return dataflow::Domain::Null;
    if (!hasNull && hasAddr) return dataflow::Domain::NotNull;
    if (hasNull && hasAddr) return dataflow::Domain::MaybeNull;
    return dataflow::Domain::Unknown;
}

void PointerAnalysis::print(std::map<std::string, PointsToSet>& PointsTo) {
    errs() << "Pointer Analysis Results:\n";
    for (auto& I : PointsTo) {
        errs() << "  " << I.first << ": { ";
        for (auto& J : I.second) {
            errs() << J << "; ";
        }
        errs() << "}\n";
    }
    errs() << "\n";
}

PointerAnalysis::PointerAnalysis(Function& F) {
    FuncName = F.getName().str();
    int NumOfOldFacts = 0;
    int NumOfNewFacts = 0;

    for (auto& Arg : F.args()) {
        if (Arg.getType()->isPointerTy()) {
            PointsToSet& S = PointsTo[variable(&Arg)];
            S.insert(address(&Arg));
        }
    }

    while (true) {
        // reset change flag for this iteration
        NullChanged = false;
        for (inst_iterator Iter = inst_begin(F), E = inst_end(F); Iter != E;
             ++Iter) {
            auto Inst = &*Iter;
            transfer(Inst, PointsTo);
        }
        NumOfNewFacts = countFacts(PointsTo);
        // continue iterating if either PointsTo facts grew or a NullState
        // changed
        if (NumOfOldFacts < NumOfNewFacts) {
            NumOfOldFacts = NumOfNewFacts;
        } else if (NullChanged) {
            // keep iterating to propagate NullState effects
        } else {
            break;
        }
    }

    // After reaching fixpoint, compute NullStates for each variable
    for (auto& I : PointsTo) {
        const auto& var = I.first;
        const PointsToSet& pts = I.second;
        bool hasNull = pts.find(std::string("NULL")) != pts.end();
        bool hasAddr = false;
        for (auto& t : pts) {
            if (t != "NULL") {
                hasAddr = true;
                break;
            }
        }
        if (hasNull && !hasAddr) {
            NullStates[var] = Domain::Null;
        } else if (!hasNull && hasAddr) {
            NullStates[var] = Domain::NotNull;
        } else if (hasNull && hasAddr) {
            NullStates[var] = Domain::MaybeNull;
        } else {
            NullStates[var] = Domain::Unknown;
        }
    }

    print(PointsTo);
    // print nullness summary
    errs() << "Nullness Summary:\n";
    for (auto& ns : NullStates) {
        errs() << "  " << ns.first << ": ";
        switch (ns.second) {
            case Domain::Unknown:
                errs() << "Unknown";
                break;
            case Domain::Null:
                errs() << "Null";
                break;
            case Domain::NotNull:
                errs() << "NotNull";
                break;
            case Domain::MaybeNull:
                errs() << "MaybeNull";
                break;
        }
        errs() << "\n";
    }

    // Post-check: iterate instructions again and emit warnings based on the
    // final PointsTo sets (more reliable than checking during transfer).
    for (inst_iterator Iter = inst_begin(F), E = inst_end(F); Iter != E;
         ++Iter) {
        Instruction* Inst = &*Iter;
        if (StoreInst* Store = dyn_cast<StoreInst>(Inst)) {
            Value* Pointer = Store->getPointerOperand();
            auto it = NullStates.find(variable(Pointer));
            if (it != NullStates.end() && (it->second == Domain::Null ||
                                           it->second == Domain::MaybeNull)) {
                errs() << "Possible null dereference (store) in " << FuncName
                       << " at: " << *Store << "\n";
            }
        } else if (LoadInst* Load = dyn_cast<LoadInst>(Inst)) {
            Value* Pointer = Load->getPointerOperand();
            auto it = NullStates.find(variable(Pointer));
            if (it != NullStates.end() && (it->second == Domain::Null ||
                                           it->second == Domain::MaybeNull)) {
                errs() << "Possible null dereference (load) in " << FuncName
                       << " at: " << *Load << "\n";
            }
        } else if (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(Inst)) {
            // GEP itself is a pointer dereference (even though it doesn't
            // access memory)
            Value* Pointer = GEP->getPointerOperand();
            auto it = NullStates.find(variable(Pointer));
            if (it != NullStates.end() && (it->second == Domain::Null ||
                                           it->second == Domain::MaybeNull)) {
                errs() << "Possible null dereference (getelementptr) in "
                       << FuncName << " at: " << *GEP << "\n";
            }
        }
    }
}

bool PointerAnalysis::alias(std::string& Ptr1, std::string& Ptr2) const {
    if (PointsTo.find(Ptr1) == PointsTo.end() ||
        PointsTo.find(Ptr2) == PointsTo.end())
        return false;
    const PointsToSet& S1 = PointsTo.at(Ptr1);
    const PointsToSet& S2 = PointsTo.at(Ptr2);

    PointsToSet Inter;
    std::set_intersection(S1.begin(), S1.end(), S2.begin(), S2.end(),
                          std::inserter(Inter, Inter.begin()));
    return !Inter.empty();
}

//===----------------------------------------------------------------------===//
// Pass registration for standalone PointerAnalysis
//===----------------------------------------------------------------------===//

const auto PASS_NAME = "PointerAnalysis";
const auto PASS_DESC = "Null pointer dereference: Constraint-Based Analysis";

struct PointerAnalysisPass : public llvm::PassInfoMixin<PointerAnalysisPass> {
    llvm::PreservedAnalyses run(llvm::Module& M,
                                llvm::ModuleAnalysisManager& AM) {
        llvm::outs() << "Running " << PASS_DESC << " on module " << M.getName()
                     << "\n";

        for (auto& F : M) {
            if (F.isDeclaration()) continue;
            llvm::outs() << "Running " << PASS_NAME << " on " << F.getName()
                         << "\n";
            // construct the analysis which prints its results
            PointerAnalysis PA(F);
        }

        return llvm::PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, PASS_NAME, "1.0.0",
            [](llvm::PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                    [](llvm::StringRef Name, llvm::ModulePassManager& MPM,
                       llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                        if (Name == PASS_NAME) {
                            MPM.addPass(PointerAnalysisPass());
                            return true;
                        }
                        return false;
                    });
            }};
}

}  // namespace dataflow
