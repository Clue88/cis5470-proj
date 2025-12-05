#include <stdlib.h>

int main() {
  int* p = NULL;
  free(p);  // no-op
  return 0;
}
