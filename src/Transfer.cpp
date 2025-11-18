#include "DoubleFreeAnalysis.h"
#include "Utils.h"

namespace dataflow {

/**
 * @brief Update the abstract state for a variable by joining with the new value.
 */
static void updateDomain(Memory& NOut, const std::string& Name, Domain* NewDom) {
  auto It = NOut.find(Name);
  if (It == NOut.end()) {
    NOut[Name] = new Domain(NewDom->Value);
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
    PointerAnalysis* PA,
    SetVector<Value*> PointerSet) {
  // Copy In into NOut as a default
  for (const auto& kv : *In) {
    NOut[kv.first] = new Domain(kv.second->Value);
  }

  if (auto* Call = dyn_cast<CallInst>(Inst)) {
    if (Call->getCalledFunction()) {
      auto Name = Call->getCalledFunction()->getName();

      // malloc-like: mark result Live
      if (Name.equals("malloc") || Name.equals("calloc") || Name.equals("realloc")) {
        if (Call->getType()->isPointerTy()) {
          NOut[variable(Call)] = new Domain(Domain::Live);
        }
        return;
      }

      // free-like: mark the slot Freed
      if (Name.equals("free")) {
        if (Call->arg_size() >= 1) {
          Value* Arg = Call->getArgOperand(0);
          std::string argName = variable(Arg);

          NOut[variable(Arg)] = new Domain(Domain::Freed);

          for (Value* V : PointerSet) {
            if (!V->getType()->isPointerTy()) {
              continue;
            }

            std::string vName = variable(V);
            if (vName == argName) {
              continue;
            }

            if (PA->alias(argName, vName)) {
              NOut[vName] = new Domain(Domain::Freed);
            }
          }
        }
      }
      return;
    }

    // Set domain for other calls to Uninit
    if (Call->getType()->isPointerTy()) {
      NOut[variable(Call)] = new Domain(Domain::Uninit);
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
    updateDomain(NOut, variable(Load), D);
    return;
  }

  if (auto* Store = dyn_cast<StoreInst>(Inst)) {
    Value* Val = Store->getValueOperand();
    Value* Ptr = Store->getPointerOperand();

    if (!Val->getType()->isPointerTy()) {
      return;
    }

    Domain* DVal = getOrExtract(In, Val);
    updateDomain(NOut, variable(Ptr), DVal);
    return;
  }
}

}  // namespace dataflow
