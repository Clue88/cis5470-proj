#include "Domain.h"

//===----------------------------------------------------------------------===//
// Abstract Domain Implementation
//===----------------------------------------------------------------------===//

namespace dataflow {

Domain::Domain() {
  Value = Uninit;
  Nstate = Unknown;
}
Domain::Domain(Element V) {
  Value = V;
  Nstate = Unknown;
}
Domain::Domain(Element V, NullState N) {
  Value = V;
  Nstate = N;
}

Domain::Domain(const Domain &Other) {
  Value = Other.Value;
  Nstate = Other.Nstate;
}

Domain* Domain::join(Domain* E1, Domain* E2) {
  using E = Domain::Element;
  using N = Domain::NullState;
  if (E1->Value == E::Uninit) {
    return new Domain(E2->Value, E2->Nstate);
  }
  if (E2->Value == E::Uninit) {
    return new Domain(E1->Value, E1->Nstate);
  }
  if (E1->Value == E::MaybeFreed || E2->Value == E::MaybeFreed) {
    // join nullness
    NullState jn;
    if (E1->Nstate == N::Unknown) jn = E2->Nstate;
    else if (E2->Nstate == N::Unknown) jn = E1->Nstate;
    else if (E1->Nstate == N::MaybeNull || E2->Nstate == N::MaybeNull)
      jn = N::MaybeNull;
    else if (E1->Nstate == E2->Nstate)
      jn = E1->Nstate;
    else
      jn = N::MaybeNull;
    return new Domain(E::MaybeFreed, jn);
  }
  if (E1->Value == E::Live && E2->Value == E::Live) {
    // both live -> preserve nullness
    NullState jn;
    if (E1->Nstate == N::Unknown) jn = E2->Nstate;
    else if (E2->Nstate == N::Unknown) jn = E1->Nstate;
    else if (E1->Nstate == E2->Nstate)
      jn = E1->Nstate;
    else
      jn = N::MaybeNull;
    return new Domain(E::Live, jn);
  }
  if (E1->Value == E::Freed && E2->Value == E::Freed) {
    NullState jn;
    if (E1->Nstate == N::Unknown) jn = E2->Nstate;
    else if (E2->Nstate == N::Unknown) jn = E1->Nstate;
    else if (E1->Nstate == E2->Nstate)
      jn = E1->Nstate;
    else
      jn = N::MaybeNull;
    return new Domain(E::Freed, jn);
  }
  // different allocation statuses -> MaybeFreed and join nullness
  NullState jn;
  if (E1->Nstate == N::Unknown) jn = E2->Nstate;
  else if (E2->Nstate == N::Unknown) jn = E1->Nstate;
  else if (E1->Nstate == N::MaybeNull || E2->Nstate == N::MaybeNull)
    jn = N::MaybeNull;
  else if (E1->Nstate == E2->Nstate)
    jn = E1->Nstate;
  else
    jn = N::MaybeNull;
  return new Domain(E::MaybeFreed, jn);
}

bool Domain::equal(Domain E1, Domain E2) {
  return E1.Value == E2.Value && E1.Nstate == E2.Nstate;
}

void Domain::print(raw_ostream& O) {
  switch (Value) {
    case Uninit:
      O << "Uninit";
      break;
    case Live:
      O << "Live";
      break;
    case Freed:
      O << "Freed";
      break;
    case MaybeFreed:
      O << "MaybeFreed";
      break;
  }

  O << "/";
  switch (Nstate) {
    case Unknown:
      O << "Unknown";
      break;
    case Null:
      O << "Null";
      break;
    case NotNull:
      O << "NotNull";
      break;
    case MaybeNull:
      O << "MaybeNull";
      break;
  }
}

raw_ostream& operator<<(raw_ostream& O, Domain V) {
  V.print(O);
  return O;
}

};  // namespace dataflow
