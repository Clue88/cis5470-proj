#include <stdlib.h>

int main() {
  int* p = (int*)malloc(sizeof(int));
  if (!p) {
    return 0;
  }

  int x = 5;

  if (x > 0) {
    free(p);
  } else {
  }

  free(p);  // potential double-free

  return 0;
}
