#include "DoubleFreeAnalysis.h"
#include "Utils.h"

namespace dataflow {

/**
 * @brief Is the given instruction a user input?
 *
 * @param Inst The instruction to check.
 * @return true If it is a user input, false otherwise.
 */
bool isInput(Instruction* Inst) {
  if (auto Call = dyn_cast<CallInst>(Inst)) {
    if (auto Fun = Call->getCalledFunction()) {
      return (Fun->getName().equals("getchar") || Fun->getName().equals("fgetc"));
    }
  }
  return false;
}

/**
 * @brief Is the given call instruction alloc-like?
 * 
 * @param Call The call instruction to check.
 * @return true if it is alloc-like, i.e., `malloc`, `calloc`, etc., false otherwise.
 */
static bool isAllocLike(CallInst* Call) {
  if (auto* Fun = Call->getCalledFunction()) {
    auto Name = Fun->getName();
    return Name.equals("malloc") || Name.equals("calloc") || Name.equals("realloc");
  }
  return false;
}

/**
 * @brief Is the given call instruction free-like?
 * 
 * @param Call The call instruction to check.
 * @return true if it is free-like, i.e., `free` or some other variant, false otherwise.
 */
static bool isFreeLike(CallInst* Call) {
  if (auto* Fun = Call->getCalledFunction()) {
    auto Name = Fun->getName();
    return Name.equals("free");
  }
  return false;
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

  // We only care about pointer-typed values
  auto updatePtr = [&](Value* V, Domain::Element E) {
    if (!V->getType()->isPointerTy()) {
      return;
    }
    NOut[variable(V)] = new Domain(E);
  };

  if (auto* Call = dyn_cast<CallInst>(Inst)) {
    // --- Allocation ---
    if (isAllocLike(Call)) {
      if (Call->getType()->isPointerTy()) {
        updatePtr(Call, Domain::Live);
      }
      return;
    }

    // --- Free ---
    if (isFreeLike(Call)) {
      if (Call->arg_size() < 1) {
        return;
      }
      Value* Ptr = Call->getArgOperand(0);
      Domain* Before = getOrExtract(In, Ptr);

      // If domain is Uninit, treat as Freed.
      Domain::Element NewState;
      switch (Before->Value) {
        case Domain::Uninit:
          NewState = Domain::Freed;
          break;
        case Domain::Live:
          NewState = Domain::Freed;
          break;
        case Domain::Freed:
          NewState = Domain::Freed;
          break;
        case Domain::MaybeFreed:
          NewState = Domain::MaybeFreed;
          break;
      }

      updatePtr(Ptr, NewState);
      return;
    }

    // Other calls that return pointers update to MaybeFreed
    if (Call->getType()->isPointerTy()) {
      updatePtr(Call, Domain::MaybeFreed);
    }
    return;
  }

  // --- Phi nodes ---
  if (auto* Phi = dyn_cast<PHINode>(Inst)) {
    if (Phi->getType()->isPointerTy()) {
      NOut[variable(Phi)] = evalCopyLike(Phi, In);
    }
    return;
  }

  // --- Pointer-copying instructions ---
  if (auto* Cast = dyn_cast<CastInst>(Inst)) {
    if (Cast->getType()->isPointerTy() || Cast->getOperand(0)->getType()->isPointerTy()) {
      NOut[variable(Cast)] = evalCopyLike(Cast, In);
    }
    return;
  }

  if (auto* GEP = dyn_cast<GetElementPtrInst>(Inst)) {
    NOut[variable(GEP)] = evalCopyLike(GEP, In);
    return;
  }

  if (auto* Load = dyn_cast<LoadInst>(Inst)) {
    if (Load->getType()->isPointerTy()) {
      // Take freedness of the memory we load from
      NOut[variable(Load)] = evalCopyLike(Load->getPointerOperand(), In);
    }
  }

  if (auto* Store = dyn_cast<StoreInst>(Inst)) {
    // Ignore stores for now
    return;
  }
}

}  // namespace dataflow
