#include <stdlib.h>

int main() {
  int* p = (int*)malloc(sizeof(int));
  if (!p) {
    return 0;
  }

  free(p);
  free(p);  // double free

  return 0;
}
