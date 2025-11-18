#include "DoubleFreeAnalysis.h"
#include "Utils.h"

namespace dataflow {

/**
 * @brief Extract the underlying slot being freed.
 *
 * A `free` call in LLVM IR rarely receives the original pointer variable
 * directly — it is usually passed something like:
 *
 *     %raw    = load i32*, i32** %p
 *     %cast   = bitcast i32* %raw to i8*
 *     call void @free(i8* %cast)
 *
 * In this example, the *real* object we want to update in the abstract
 * memory is `%p`, not `%cast` or `%raw`.
 *
 * This helper walks backward through bitcasts and GEPs to recover the
 * original value, and if that value is a `load`, returns its pointer
 * operand (the slot being loaded from).
 *
 * Returns:
 *   - The Value* representing the storage location (e.g. `%p`) whose
 *     abstract freedness state should be updated after a `free`.
 *   - `nullptr` if no such slot can be determined.
 */
static Value* getFreeBaseSlot(Value* V) {
  // Strip bitcasts and GEPs
  while (true) {
    if (auto* BC = dyn_cast<BitCastInst>(V)) {
      V = BC->getOperand(0);
      continue;
    }
    if (auto* GEP = dyn_cast<GetElementPtrInst>(V)) {
      V = GEP->getPointerOperand();
      continue;
    }
    break;
  }

  // If we now have a load, its pointer operand is the slot
  if (auto* L = dyn_cast<LoadInst>(V)) {
    return L->getPointerOperand();  // e.g. %p
  }

  return nullptr;
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

          NOut[variable(Arg)] = new Domain(Domain::Freed);

          // Try to find the base slot (like %p) and mark it Freed too
          if (Value* Slot = getFreeBaseSlot(Arg)) {
            NOut[variable(Slot)] = new Domain(Domain::Freed);
          }
        }
        return;
      }
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
    Value* Ptr = Load->getPointerOperand();

    if (!Load->getType()->isPointerTy()) {
      return;
    }

    NOut[variable(Load)] = getOrExtract(In, Ptr);
  }

  if (auto* Store = dyn_cast<StoreInst>(Inst)) {
    Value* Val = Store->getValueOperand();
    Value* Ptr = Store->getPointerOperand();

    if (!Val->getType()->isPointerTy()) {
      return;
    }

    // Store the domain of the value into the pointer slot
    Domain* DVal = getOrExtract(In, Val);
    NOut[variable(Ptr)] = new Domain(DVal->Value);
  }
}

}  // namespace dataflow
