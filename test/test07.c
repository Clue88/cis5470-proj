#include <stdlib.h>

int main(int argc, char** argv) {
  int* p1 = (int*)malloc(sizeof(int));
  int* p2 = (int*)malloc(sizeof(int));
  int* p;

  if (argc > 1) {
    p = p1;
  } else {
    p = p2;
  }

  free(p);

  if (argc > 2) {
    p = p1;
  } else {
    p = p2;
  }

  free(p);  // potential double-free

  return 0;
}
