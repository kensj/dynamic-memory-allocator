#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "sfmm.h"

#define MAX_HEAP_SIZE (20 * (1 << 20))

int main(int argc, char *argv[]) {
    sf_mem_init();
    sf_malloc(sizeof(short));
    //sf_free(pointer);
    return EXIT_SUCCESS;
}
