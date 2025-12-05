#include <stdlib.h>

int main() {
  int* p = malloc(sizeof(int));
  int* q = p;  // alias
  free(p);
  return *q;  // UAF (load through alias)
}
