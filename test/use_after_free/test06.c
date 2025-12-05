#include <stdlib.h>

int main(int cond) {
  int* p = malloc(sizeof(int));
  if (cond) {
    free(p);
  }
  return *p;  // possibly UAF
}
