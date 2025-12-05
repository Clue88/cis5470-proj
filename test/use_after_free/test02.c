#include <stdlib.h>

int main() {
  int* p = malloc(sizeof(int));
  free(p);
  *p = 42;  // UAF (store)
  return 0;
}
