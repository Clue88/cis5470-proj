#include <stdlib.h>

int main() {
  int* p = malloc(sizeof(int));
  free(p);
  int x = *p;  // UAF (load)
  return x;
}
