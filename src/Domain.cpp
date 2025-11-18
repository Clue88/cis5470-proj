#include "Domain.h"

//===----------------------------------------------------------------------===//
// Abstract Domain Implementation
//===----------------------------------------------------------------------===//

namespace dataflow {

Domain::Domain() {
  Value = Uninit;
}
Domain::Domain(Element V) {
  Value = V;
}

Domain* Domain::join(Domain* E1, Domain* E2) {
  using E = Domain::Element;
  if (E1->Value == E::Uninit) {
    return new Domain(E2->Value);
  }
  if (E2->Value == E::Uninit) {
    return new Domain(E1->Value);
  }
  if (E1->Value == E::MaybeFreed || E2->Value == E::MaybeFreed) {
    return new Domain(E::MaybeFreed);
  }
  if (E1->Value == E::Live && E2->Value == E::Live) {
    return new Domain(E::Live);
  }
  if (E1->Value == E::Freed && E2->Value == E::Freed) {
    return new Domain(E::Freed);
  }
  return new Domain(E::MaybeFreed);
}

bool Domain::equal(Domain E1, Domain E2) {
  return E1.Value == E2.Value;
}

void Domain::print(raw_ostream& O) {
  switch (Value) {
    case Uninit:
      O << "Uninit    ";
      break;
    case Live:
      O << "Live      ";
      break;
    case Freed:
      O << "Freed     ";
      break;
    case MaybeFreed:
      O << "MaybeFreed";
      break;
  }
}

raw_ostream& operator<<(raw_ostream& O, Domain V) {
  V.print(O);
  return O;
}

};  // namespace dataflow
