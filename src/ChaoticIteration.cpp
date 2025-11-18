#include "DoubleFreeAnalysis.h"
#include "Utils.h"

namespace dataflow {

/**
 * @brief Get the Predecessors of a given instruction in the control-flow graph.
 *
 * @param Inst The instruction to get the predecessors of.
 * @return Vector of all predecessors of Inst.
 */
std::vector<Instruction*> getPredecessors(Instruction* Inst) {
  std::vector<Instruction*> Ret;
  auto Block = Inst->getParent();
  for (auto Iter = Block->rbegin(), End = Block->rend(); Iter != End; ++Iter) {
    if (&(*Iter) == Inst) {
      ++Iter;
      if (Iter != End) {
        Ret.push_back(&(*Iter));
        return Ret;
      }
      for (auto Pre = pred_begin(Block), BE = pred_end(Block); Pre != BE; ++Pre) {
        Ret.push_back(&(*((*Pre)->rbegin())));
      }
      return Ret;
    }
  }
  return Ret;
}

/**
 * @brief Get the successors of a given instruction in the control-flow graph.
 *
 * @param Inst The instruction to get the successors of.
 * @return Vector of all successors of Inst.
 */
std::vector<Instruction*> getSuccessors(Instruction* Inst) {
  std::vector<Instruction*> Ret;
  auto Block = Inst->getParent();
  for (auto Iter = Block->begin(), End = Block->end(); Iter != End; ++Iter) {
    if (&(*Iter) == Inst) {
      ++Iter;
      if (Iter != End) {
        Ret.push_back(&(*Iter));
        return Ret;
      }
      for (auto Succ = succ_begin(Block), BS = succ_end(Block); Succ != BS; ++Succ) {
        Ret.push_back(&(*((*Succ)->begin())));
      }
      return Ret;
    }
  }
  return Ret;
}

/**
 * @brief Joins two Memory objects (Mem1 and Mem2), accounting for Domain
 * values.
 *
 * @param Mem1 First memory.
 * @param Mem2 Second memory.
 * @return The joined memory.
 */
Memory* join(Memory* Mem1, Memory* Mem2) {
  /**
   * If some instruction with domain D is either in Mem1 or Mem2, but not in
   *   both, add it with domain D to the Result.
   * If some instruction is present in Mem1 with domain D1 and in Mem2 with
   *   domain D2, then Domain::join D1 and D2 to find the new domain D,
   *   and add instruction I with domain D to the Result.
   */
  auto* Result = new Memory();

  for (const auto& kv : *Mem1) {
    Result->emplace(kv.first, new Domain(kv.second->Value));
  }

  for (const auto& kv : *Mem2) {
    const auto& var = kv.first;
    Domain* d2 = kv.second;

    auto it = Result->find(var);
    if (it == Result->end()) {
      // key is not in Mem1
      Result->emplace(var, new Domain(d2->Value));
    } else {
      // key is in both, so replace with the joined domain
      Domain* d1 = it->second;
      Domain* dj = Domain::join(d1, d2);
      it->second = dj;
    }
  }

  return Result;
}

void DoubleFreeAnalysis::flowIn(Instruction* Inst, Memory* InMem) {
  /**
   * For each predecessor Pred of instruction Inst, do the following:
   *   + Get the Out Memory of Pred using OutMap.
   *   + Join the Out Memory with InMem.
   */
  InMem->clear();

  for (auto* Pred : getPredecessors(Inst)) {
    Memory* PredOut = OutMap[Pred];

    if (InMem->empty()) {
      for (const auto& kv : *PredOut) {
        (*InMem)[kv.first] = new Domain(kv.second->Value);
      }
    } else {
      Memory* Joined = join(InMem, PredOut);
      InMem->clear();
      for (const auto& kv : *Joined) {
        (*InMem)[kv.first] = new Domain(kv.second->Value);
      }
    }
  }
}

/**
 * @brief This function returns true if the two memories Mem1 and Mem2 are
 * equal.
 *
 * @param Mem1 First memory
 * @param Mem2 Second memory
 * @return true if the two memories are equal, false otherwise.
 */
bool equal(Memory* Mem1, Memory* Mem2) {
  /**
   * If any instruction I is present in one of Mem1 or Mem2,
   *   but not in both and the Domain of I is not UnInit, the memories are
   *   unequal.
   * If any instruction I is present in Mem1 with domain D1 and in Mem2
   *   with domain D2, if D1 and D2 are unequal, then the memories are unequal.
   */
  for (const auto& kv : *Mem1) {
    const auto& var = kv.first;
    Domain* d1 = kv.second;

    auto it2 = Mem2->find(var);
    if (it2 == Mem2->end()) {
      if (d1->Value != Domain::Uninit) {
        return false;
      }
    } else {
      Domain* d2 = it2->second;
      if (!Domain::equal(*d1, *d2)) {
        return false;
      }
    }
  }

  for (const auto& kv : *Mem2) {
    const auto& var = kv.first;
    Domain* d2 = kv.second;

    auto it1 = Mem1->find(var);
    if (it1 == Mem1->end()) {
      if (d2->Value != Domain::Uninit) {
        return false;
      }
    }
  }

  return true;
}

void DoubleFreeAnalysis::flowOut(
    Instruction* Inst, Memory* Pre, Memory* Post, SetVector<Instruction*>& WorkSet) {
  /**
   * For each given instruction, merge abstract domain from pre-transfer memory
   * and post-transfer memory, and update the OutMap.
   * If the OutMap changed then also update the WorkSet.
   */
  auto* OutNew = new Memory();

  for (const auto& kv : *Pre) {
    (*OutNew)[kv.first] = new Domain(kv.second->Value);
  }

  for (const auto& kv : *Post) {
    (*OutNew)[kv.first] = new Domain(kv.second->Value);
  }

  Memory* OutOld = OutMap[Inst];
  bool changed = !equal(OutOld, OutNew);

  if (changed) {
    OutOld->clear();
    for (const auto& kv : *OutNew) {
      (*OutOld)[kv.first] = new Domain(kv.second->Value);
    }
    for (auto* Succ : getSuccessors(Inst)) {
      WorkSet.insert(Succ);
    }
  }
}

void DoubleFreeAnalysis::doAnalysis(Function& F, PointerAnalysis* PA) {
  SetVector<Instruction*> WorkSet;
  SetVector<Value*> PointerSet;
  /**
   * First, find the arguments of function call and instantiate abstract domain values
   * for each argument.
   * Initialize the WorkSet and PointerSet with all the instructions in the function.
   * The rest of the implementation is almost similar to the previous lab.
   *
   * While the WorkSet is not empty:
   * - Pop an instruction from the WorkSet.
   * - Construct it's Incoming Memory using flowIn.
   * - Evaluate the instruction using transfer and create the OutMemory.
   *   Note that the transfer function takes two additional arguments compared to previous lab:
   *   the PointerAnalysis object and the populated PointerSet.
   * - Use flowOut along with the previous Out memory and the current Out
   *   memory, to check if there is a difference between the two to update the
   *   OutMap and add all successors to WorkSet.
   */

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    WorkSet.insert(&(*I));
    PointerSet.insert(&(*I));
  }

  auto isEntryInst = [&](Instruction* Inst) { return getPredecessors(Inst).empty(); };

  while (!WorkSet.empty()) {
    Instruction* Inst = WorkSet.pop_back_val();

    Memory* In = InMap[Inst];
    flowIn(Inst, In);

    if (isEntryInst(Inst)) {
      for (auto& Arg : F.args()) {
        (*In)[variable(&Arg)] = new Domain(Domain::Live);
      }
    }

    Memory OutCur;
    transfer(Inst, In, OutCur, PA, PointerSet);

    flowOut(Inst, In, &OutCur, WorkSet);
  }
}

}  // namespace dataflow
