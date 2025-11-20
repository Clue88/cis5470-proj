#include <stdlib.h>

int main() {
    int *p = malloc(sizeof(int));
    free(p);
    p = NULL; // safe, resetting
    // not a double free because p is NULL
    // free(p); // would be safe but commented out
    return 0;
}
