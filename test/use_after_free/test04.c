#include <stdlib.h>

void foo(int* p) {
  if (p) {
    *p = 5;
  }
}

int main() {
  int* p = malloc(sizeof(int));
  free(p);
  foo(p);  // UAF (passing freed pointer)
  return 0;
}
