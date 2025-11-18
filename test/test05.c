#include <stdlib.h>

void g(int* p) {
  free(p);
  free(p);  // double-free
}

int main() {
  int* p = (int*)malloc(sizeof(int));
  if (!p) {
    return 0;
  }

  g(p);
  return 0;
}
