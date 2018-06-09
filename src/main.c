#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "sfmm.h"

#define MAX_HEAP_SIZE (20 * (1 << 20))

int main(int argc, char *argv[]) {
    Mem_init();
    Malloc(5000);
    return EXIT_SUCCESS;
}
