#include <stdlib.h>
#include <stdio.h>

int main() {
    int *p = NULL;
    // definite null dereference
    int x = *p;
    (void)x;
    return 0;
}
