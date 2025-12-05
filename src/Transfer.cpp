#include "DoubleFreeAnalysis.h"
#include "Utils.h"

namespace dataflow {

/**
 * @brief Update the abstract state for a variable by joining with the new value.
 */
static void updateDomain(Memory& NOut, const std::string& Name, Domain* NewDom) {
  auto It = NOut.find(Name);
  if (It == NOut.end()) {
    NOut[Name] = new Domain(*NewDom);
  } else {
    Domain* Joined = Domain::join(It->second, NewDom);
    It->second = Joined;
  }
}

/**
 * @brief Evaluate a Value to get its Domain.
 */
Domain* evalCopyLike(Value* V, const Memory* InMem) {
  // If V is a Phi, propagate from incoming values, joining
  if (auto* Phi = dyn_cast<PHINode>(V)) {
    Domain* Joined = new Domain(Domain::Uninit);
    for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
      Domain* Dom = getOrExtract(InMem, Phi->getIncomingValue(i));
      Joined = Domain::join(Joined, Dom);
    }
    return Joined;
  }

  // If V is a cast or GEP, just propagate from the operand
  if (auto* Cast = dyn_cast<CastInst>(V)) {
    return getOrExtract(InMem, Cast->getOperand(0));
  }

  if (auto* GEP = dyn_cast<GetElementPtrInst>(V)) {
    return getOrExtract(InMem, GEP->getPointerOperand());
  }

  // Fallback: look up the value itself
  return getOrExtract(InMem, V);
}

void DoubleFreeAnalysis::transfer(Instruction* Inst,
    const Memory* In,
    Memory& NOut,
    DoubleFreePointerAnalysis* PA,
    SetVector<Value*> PointerSet) {
  // Copy In into NOut as a default
  for (const auto& kv : *In) {
    NOut[kv.first] = new Domain(*kv.second);
  }

  if (auto* Call = dyn_cast<CallInst>(Inst)) {
    if (Call->getCalledFunction()) {
      auto Name = Call->getCalledFunction()->getName();

      // malloc-like: mark result Live
      if (Name.equals("malloc") || Name.equals("calloc") || Name.equals("realloc")) {
        if (Call->getType()->isPointerTy()) {
          // malloc returns a live pointer; treat as NotNull by default
          NOut[variable(Call)] = new Domain(Domain::Live, Domain::NotNull);
        }
        return;
      }

      // free-like: mark the slot Freed
      if (Name.equals("free")) {
        if (Call->arg_size() >= 1) {
          Value* Arg = Call->getArgOperand(0);
          std::string argName = variable(Arg);
          // Preserve existing nullness when marking Freed
          Domain* Prev = getOrExtract(In, Arg);
          NOut[variable(Arg)] = new Domain(Domain::Freed, Prev->Nstate);

          for (Value* V : PointerSet) {
            if (!V->getType()->isPointerTy()) {
              continue;
            }

            std::string vName = variable(V);
            if (vName == argName) {
              continue;
            }

            if (PA->alias(argName, vName)) {
              Domain* PrevV = getOrExtract(In, V);
              NOut[vName] = new Domain(Domain::Freed, PrevV->Nstate);
            }
          }
        }
      }
      return;
    }

    // Set domain for other calls to Uninit
    if (Call->getType()->isPointerTy()) {
      NOut[variable(Call)] = new Domain(Domain::Uninit, Domain::Unknown);
    }
    return;
  }

  // --- Phi nodes ---
  if (auto* Phi = dyn_cast<PHINode>(Inst)) {
    if (Phi->getType()->isPointerTy()) {
      Domain* D = evalCopyLike(Phi, In);
      updateDomain(NOut, variable(Phi), D);
    }
    return;
  }

  // --- Pointer-copying instructions ---
  if (auto* Cast = dyn_cast<CastInst>(Inst)) {
    if (Cast->getType()->isPointerTy() || Cast->getOperand(0)->getType()->isPointerTy()) {
      Domain* D = evalCopyLike(Cast, In);
      updateDomain(NOut, variable(Cast), D);
    }
    return;
  }

  if (auto* GEP = dyn_cast<GetElementPtrInst>(Inst)) {
    if (GEP->getType()->isPointerTy()) {
      Domain* D = evalCopyLike(GEP, In);
      updateDomain(NOut, variable(GEP), D);
    }
    return;
  }

  if (auto* Load = dyn_cast<LoadInst>(Inst)) {
    Value* Ptr = Load->getPointerOperand();

    if (!Load->getType()->isPointerTy()) {
      return;
    }

    Domain* D = getOrExtract(In, Ptr);
    // If we are loading from a definitely-null pointer, set loaded value to Null
    updateDomain(NOut, variable(Load), D);
    return;
  }

  if (auto* Store = dyn_cast<StoreInst>(Inst)) {
    Value* Val = Store->getValueOperand();
    Value* Ptr = Store->getPointerOperand();

    if (!Val->getType()->isPointerTy()) {
      return;
    }
    Domain* DVal;
    if (isa<ConstantPointerNull>(Val)) {
      // Explicit NULL constant
      DVal = new Domain(Domain::Uninit, Domain::Null);
    } else {
      DVal = getOrExtract(In, Val);
    }
    updateDomain(NOut, variable(Ptr), DVal);
    return;
  }
}

}  // namespace dataflow
