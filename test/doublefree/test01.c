#include <stdlib.h>

int main() {
    int *p = malloc(sizeof(int));
    free(p);
    // double free
    free(p);
    return 0;
}
