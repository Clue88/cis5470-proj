#include <stdlib.h>

int main() {
  int* p = (int*)malloc(sizeof(int));
  if (!p) {
    return 0;
  }

  *p = 42;
  free(p);  // no warning

  return 0;
}
