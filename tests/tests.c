#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"

#define GET_FREE_HEAD(foot) ((sf_free_header*)((char*)foot - (foot->block_size << 4) + SF_FOOTER_SIZE))

Test(sf_memsuite, Init_free_list, .init = Mem_init, .fini = Mem_fini) {
  sf_footer* footer = (sf_footer*)((char*)sbrk(0) - SF_FOOTER_SIZE);
  sf_free_header* head = GET_FREE_HEAD(footer);
  
  cr_assert(footer->block_size == head->header.block_size, "Block sizes do not match!");
  cr_assert(((sf_footer*)((char*)head + PAGE_SIZE - SF_FOOTER_SIZE)) == footer, "Footer location incorrect!");
}

