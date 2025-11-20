#include "PointerAnalysis.h"

#include "Utils.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"

namespace dataflow {

void PointerAnalysis::transfer(Instruction* Inst, PointsToInfo& PointsTo) {
  if (AllocaInst* Alloca = dyn_cast<AllocaInst>(Inst)) {
    PointsToSet& S = PointsTo[variable(Alloca)];
    S.insert(address(Alloca));

  } else if (StoreInst* Store = dyn_cast<StoreInst>(Inst)) {
    Value* Pointer = Store->getPointerOperand();
    Value* Value = Store->getValueOperand();

    // Check for stores through a pointer that might be NULL
    PointsToSet& PtrSet = PointsTo[variable(Pointer)];
    if (PtrSet.find(std::string("NULL")) != PtrSet.end()) {
      errs() << "Possible null dereference (store) at: " << *Store << "\n";
    }

    // If the RHS is not a pointer, nothing to update
    if (!Value->getType()->isPointerTy())
      return;

    // RHS could be an explicit null constant
    PointsToSet R;
    if (isa<ConstantPointerNull>(Value)) {
      R.insert(std::string("NULL"));
    } else {
      R = PointsTo[variable(Value)];
    }

    PointsToSet& L = PointsTo[variable(Pointer)];
    for (auto& I : L) {
      PointsToSet& S = PointsTo[I];
      PointsToSet Result;
      std::set_union(S.begin(), S.end(), R.begin(), R.end(), std::inserter(Result, Result.begin()));
      PointsTo[I] = Result;
    }

  } else if (LoadInst* Load = dyn_cast<LoadInst>(Inst)) {
    if (!Load->getType()->isPointerTy()) {
      return;
    }
    // Check whether we are dereferencing a pointer that might be NULL
    Value* Ptr = Load->getPointerOperand();
    PointsToSet& PSet = PointsTo[variable(Ptr)];
    if (PSet.find(std::string("NULL")) != PSet.end()) {
      errs() << "Possible null dereference (load) at: " << *Load << "\n";
    }

    std::string VariableName = variable(Load->getPointerOperand());
    PointsToSet& R = PointsTo[VariableName];
    PointsToSet Result;
    for (auto& I : R) {
      PointsToSet& S = PointsTo[I];
      std::set_union(S.begin(), S.end(), Result.begin(), Result.end(), std::inserter(Result, Result.begin()));
    }
    PointsTo[variable(Load)] = Result;

  } else if (auto* Call = dyn_cast<CallInst>(Inst)) {
    if (Call->getType()->isPointerTy()) {
      PointsToSet& S = PointsTo[variable(Call)];
      S.insert(address(Call));
    }

  } else if (auto* Cast = dyn_cast<CastInst>(Inst)) {
    if (Cast->getType()->isPointerTy() && Cast->getOperand(0)->getType()->isPointerTy()) {
      PointsTo[variable(Cast)] = PointsTo[variable(Cast->getOperand(0))];
    }

  } else if (auto* GEP = dyn_cast<GetElementPtrInst>(Inst)) {
    if (GEP->getType()->isPointerTy()) {
      PointsTo[variable(GEP)] = PointsTo[variable(GEP->getPointerOperand())];
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
      std::set_union(S.begin(),
          S.end(),
          Result.begin(),
          Result.end(),
          std::inserter(Result, Result.begin()));
    }
    PointsTo[variable(Phi)] = Result;
  }
}

int PointerAnalysis::countFacts(PointsToInfo& PointsTo) {
  int N = 0;
  for (auto& I : PointsTo)
    N += I.second.size();
  return N;
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
  int NumOfOldFacts = 0;
  int NumOfNewFacts = 0;

  for (auto& Arg : F.args()) {
    if (Arg.getType()->isPointerTy()) {
      PointsToSet& S = PointsTo[variable(&Arg)];
      S.insert(address(&Arg));
    }
  }

  while (true) {
    for (inst_iterator Iter = inst_begin(F), E = inst_end(F); Iter != E; ++Iter) {
      auto Inst = &*Iter;
      transfer(Inst, PointsTo);
    }
    NumOfNewFacts = countFacts(PointsTo);
    if (NumOfOldFacts < NumOfNewFacts)
      NumOfOldFacts = NumOfNewFacts;
    else
      break;
  }
  print(PointsTo);
}

bool PointerAnalysis::alias(std::string& Ptr1, std::string& Ptr2) const {
  if (PointsTo.find(Ptr1) == PointsTo.end() || PointsTo.find(Ptr2) == PointsTo.end())
    return false;
  const PointsToSet& S1 = PointsTo.at(Ptr1);
  const PointsToSet& S2 = PointsTo.at(Ptr2);

  PointsToSet Inter;
  std::set_intersection(
      S1.begin(), S1.end(), S2.begin(), S2.end(), std::inserter(Inter, Inter.begin()));
  return !Inter.empty();
}

};  // namespace dataflow
