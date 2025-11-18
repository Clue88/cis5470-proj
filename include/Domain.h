#ifndef DOMAIN_H
#define DOMAIN_H

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace dataflow {

//===----------------------------------------------------------------------===//
// Abstract Domain Implementation
//===----------------------------------------------------------------------===//

/*
 * Abstract domain.
 * * `Uninit` - No info (bottom)
 * * `Live` -  Definitely allocated and not yet freed
 * * `Freed` - Definitely freed
 * * `MaybeFreed` - Might be freed, might not (top)
 */
class Domain {
 public:
  enum Element {
    Uninit,
    Live,
    Freed,
    MaybeFreed
  };
  Domain();
  Domain(Element V);
  Element Value;

  static Domain* join(Domain* E1, Domain* E2);
  static bool equal(Domain E1, Domain E2);
  void print(raw_ostream& O);
};

raw_ostream& operator<<(raw_ostream& O, Domain V);

}  // namespace dataflow

#endif  // DOMAIN_H
