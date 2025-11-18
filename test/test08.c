#include <stdlib.h>

int main() {
  int* p = (int*)malloc(sizeof(int));
  if (!p) {
    return 0;
  }

  int* q = p;

  free(p);
  q = NULL;

  return 0;  // no warning
}
