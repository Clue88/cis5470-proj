#ifndef POINTER_ANALYSIS_H
#define POINTER_ANALYSIS_H

#include <map>
#include <set>

#include "Domain.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

using namespace llvm;

namespace dataflow {

//===----------------------------------------------------------------------===//
// Pointer Analysis
//===----------------------------------------------------------------------===//

using PointsToSet = std::set<std::string>;

/**
 * @brief PointsToInfo represents the set of allocation sites a variable can
 * point to.
 *
 */
using PointsToInfo = std::map<std::string, PointsToSet>;

// Set of pointer variable names that are known NotNull in a given context
using GuardedPtrSet = std::set<std::string>;

class PointerAnalysis {
   public:
    /**
     * @brief Build a points-to graph
     *
     * This constructor applies the pointer analysis transfer function
     * on each instruction in function F.
     *
     * @param F The function for which pointer analysis is done
     */
    PointerAnalysis(Function& F);

    /**
     * @brief If the instruction is memory allocation, store, or load, updates
     * the points-to sets.
     *
     * @param Inst The instruction to be analyzed for aliasing
     * @param PointsTo The set of
     */
    void transfer(Instruction* Inst, PointsToInfo& PointsTo);

    /**
     * @brief Returns true if two pointers are aliased
     *
     * @param Ptr1 First pointer
     * @param Ptr2 Second pointer
     * @return bool
     */
    bool alias(std::string& Ptr1, std::string& Ptr2) const;

   private:
    PointsToInfo PointsTo;

    // Name of the function being analyzed (used for clearer warnings)
    std::string FuncName;

    // Nullness state per variable (uses Domain::NullState values)
    std::map<std::string, dataflow::Domain::NullState> NullStates;
    // Flag set during transfer when a NullState changes (used for fixpoint)
    bool NullChanged = false;

    // Per-basic-block set of pointers guarded by null checks
    // If a pointer is in GuardedNotNull[BB], it's known NotNull in BB
    std::map<BasicBlock*, GuardedPtrSet> GuardedNotNull;

    // Compute the NullState for a variable from the PointsTo map
    Domain::NullState computeNullState(const std::string& var,
                                       PointsToInfo& PointsTo);

    // Analyze null-check branches and populate GuardedNotNull
    void analyzeNullGuards(Function& F);

    // Check if a pointer is guarded (known NotNull) at a given instruction
    bool isGuardedNotNull(const std::string& ptrVar, Instruction* Inst);

    /**
     * @brief
     *
     * @param PointsTo
     * @return int
     */
    int countFacts(PointsToInfo& PointsTo);

    /**
     * @brief
     *
     * @param PointsTo
     */
    void print(std::map<std::string, PointsToSet>& PointsTo);
};
};  // namespace dataflow

#endif  // POINTER_ANALYSIS_H
