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
 * Evaluate a PHINode to get its Domain.
 *
 * @param Phi PHINode to evaluate
 * @param InMem InMemory of Phi
 * @return Domain of Phi
 */
Domain* eval(PHINode* Phi, const Memory* InMem) {
  if (auto ConstantVal = Phi->hasConstantValue()) {
    return new Domain(extractFromValue(ConstantVal));
  }

  Domain* Joined = new Domain(Domain::Uninit);

  for (unsigned int i = 0; i < Phi->getNumIncomingValues(); i++) {
    auto Dom = getOrExtract(InMem, Phi->getIncomingValue(i));
    Joined = Domain::join(Joined, Dom);
  }
  return Joined;
}

/**
 * @brief Evaluate the +, -, * and / BinaryOperator instructions
 * using the Domain of its operands and return the Domain of the result.
 *
 * @param BinOp BinaryOperator to evaluate
 * @param InMem InMemory of BinOp
 * @return Domain of BinOp
 */
Domain* eval(BinaryOperator* BinOp, const Memory* InMem) {
  auto* L = getOrExtract(InMem, BinOp->getOperand(0));
  auto* R = getOrExtract(InMem, BinOp->getOperand(1));

  switch (BinOp->getOpcode()) {
    case Instruction::Add:
      return Domain::add(L, R);
    case Instruction::Sub:
      return Domain::sub(L, R);
    case Instruction::Mul:
      return Domain::mul(L, R);
    case Instruction::SDiv:
    case Instruction::UDiv:
      return Domain::div(L, R);
    default:
      return new Domain(Domain::MaybeZero);
  }
}

/**
 * @brief Evaluate Cast instructions.
 *
 * @param Cast Cast instruction to evaluate
 * @param InMem InMemory of Instruction
 * @return Domain of Cast
 */
Domain* eval(CastInst* Cast, const Memory* InMem) {
  return getOrExtract(InMem, Cast->getOperand(0));
}

/**
 * @brief Evaluate the ==, !=, <, <=, >=, and > Comparision operators using
 * the Domain of its operands to compute the Domain of the result.
 *
 * @param Cmp Comparision instruction to evaluate
 * @param InMem InMemory of Cmp
 * @return Domain of Cmp
 */
Domain* eval(CmpInst* Cmp, const Memory* InMem) {
  auto* L = getOrExtract(InMem, Cmp->getOperand(0));
  auto* R = getOrExtract(InMem, Cmp->getOperand(1));
  auto pred = Cmp->getPredicate();

  const auto LV = L->Value;
  const auto RV = R->Value;

  switch (pred) {
    case CmpInst::ICMP_EQ:
      // 0 == 0 -> true
      if (LV == Domain::Zero && RV == Domain::Zero) {
        return new Domain(Domain::NonZero);
      }
      // 0 == NZ -> false and vice versa
      if ((LV == Domain::Zero && RV == Domain::NonZero) ||
          (LV == Domain::NonZero && RV == Domain::Zero)) {
        return new Domain(Domain::Zero);
      }
      // anything else is unknown
      return new Domain(Domain::MaybeZero);

    case CmpInst::ICMP_NE:
      if (LV == Domain::Zero && RV == Domain::Zero) {
        return new Domain(Domain::Zero);
      }
      if ((LV == Domain::Zero && RV == Domain::NonZero) ||
          (LV == Domain::NonZero && RV == Domain::Zero)) {
        return new Domain(Domain::NonZero);
      }
      return new Domain(Domain::MaybeZero);

    default:
      return new Domain(Domain::MaybeZero);
  }
}

void DoubleFreeAnalysis::transfer(Instruction* Inst,
    const Memory* In,
    Memory& NOut,
    PointerAnalysis* PA,
    SetVector<Value*> PointerSet) {
  if (isInput(Inst)) {
    // The instruction is a user controlled input, it can have any value.
    NOut[variable(Inst)] = new Domain(Domain::MaybeZero);
  } else if (auto Phi = dyn_cast<PHINode>(Inst)) {
    // Evaluate PHI node
    NOut[variable(Phi)] = eval(Phi, In);
  } else if (auto BinOp = dyn_cast<BinaryOperator>(Inst)) {
    // Evaluate BinaryOperator
    NOut[variable(BinOp)] = eval(BinOp, In);
  } else if (auto Cast = dyn_cast<CastInst>(Inst)) {
    // Evaluate Cast instruction
    NOut[variable(Cast)] = eval(Cast, In);
  } else if (auto Cmp = dyn_cast<CmpInst>(Inst)) {
    // Evaluate Comparision instruction
    NOut[variable(Cmp)] = eval(Cmp, In);
  } else if (auto Alloca = dyn_cast<AllocaInst>(Inst)) {
    // Do nothing here.
  } else if (auto Store = dyn_cast<StoreInst>(Inst)) {
    /**
     * Store instruction can either add new variables or overwrite existing variables into memory maps.
     * To update the memory map, we rely on the points-to graph constructed in PointerAnalysis.
     *
     * To build the abstract memory map, you need to ensure all pointer references are in-sync, and
     * will converge upon a precise abstract value. To achieve this, implement the following workflow:
     *
     * Iterate through the provided PointerSet:
     *   - If there is a may-alias (i.e., `alias()` returns true) between two variables:
     *     + Get the abstract values of each variable.
     *     + Join the abstract values using Domain::join().
     *     + Update the memory map for the current assignment with the joined abstract value.
     *     + Update the memory map for all may-alias assignments with the joined abstract value.
     *
     * Hint: You may find getOperand(), getValueOperand(), and getPointerOperand() useful.
     */
    Value* Val = Store->getValueOperand();
    Value* Ptr = Store->getPointerOperand();

    if (!Val->getType()->isIntegerTy()) {
    } else {
      Domain* stored = getOrExtract(In, Val);
      Domain* joined = new Domain(stored->Value);

      std::string ptrName = variable(Ptr);

      {
        Domain* cur = getOrExtract(In, Ptr);
        joined = Domain::join(joined, cur);
      }

      for (Value* V : PointerSet) {
        if (!V->getType()->isPointerTy()) {
          continue;
        }
        std::string vName = variable(V);
        if (PA->alias(ptrName, vName)) {
          Domain* cur = getOrExtract(In, V);
          joined = Domain::join(joined, cur);
        }
      }

      NOut[ptrName] = new Domain(joined->Value);
      for (Value* V : PointerSet) {
        if (!V->getType()->isPointerTy()) {
          continue;
        }
        std::string vName = variable(V);
        if (PA->alias(ptrName, vName)) {
          NOut[vName] = new Domain(joined->Value);
        }
      }
    }
  } else if (auto Load = dyn_cast<LoadInst>(Inst)) {
    /**
     * Rely on the existing variables defined within the `In` memory to
     * know what abstract domain should be for the new variable
     * introduced by a load instruction.
     *
     * If the memory map already contains the variable, propagate the existing
     * abstract value to NOut.
     * Otherwise, initialize the memory map for it.
     *
     * Hint: You may use getPointerOperand().
     */
    Value* Ptr = Load->getPointerOperand();

    std::string lhs = variable(Load);
    auto it = In->find(lhs);
    if (it != In->end()) {
      NOut[lhs] = new Domain(it->second->Value);
    } else {
      NOut[lhs] = getOrExtract(In, Ptr);
    }
  } else if (auto Branch = dyn_cast<BranchInst>(Inst)) {
    // Analysis is flow-insensitive, so do nothing here.
  } else if (auto Call = dyn_cast<CallInst>(Inst)) {
    /**
     * Populate the NOut with an appropriate abstract domain.
     *
     * You only need to consider calls with int return type.
     */
    if (Call->getType()->isIntegerTy()) {
      NOut[variable(Call)] = new Domain(Domain::MaybeZero);
    }
  } else if (auto Return = dyn_cast<ReturnInst>(Inst)) {
    // Analysis is intra-procedural, so do nothing here.
  } else {
    errs() << "Unhandled instruction: " << *Inst << "\n";
  }
}

}  // namespace dataflow
