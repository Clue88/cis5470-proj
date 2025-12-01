#include <stdlib.h>
#include <stdio.h>

int main() {
    int a = 42;
    int *p = &a;
    // safe dereference
    int x = *p;
    (void)x;
    return 0;
}
