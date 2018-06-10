#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"

#define GET_HEAD(foot)				((sf_header*)((char*)((sf_footer*)foot) - (((sf_footer*)foot)->block_size << 4) + SF_FOOTER_SIZE))
#define GET_FOOT(head)				((sf_footer*)((char*)((sf_header*)head) + (((sf_header*)head)->block_size << 4) - SF_FOOTER_SIZE))

Test(sf_memsuite, Init_free_list, .init = Mem_init, .fini = Mem_fini) {
  sf_footer* footer = (sf_footer*)((char*)sbrk(0) - SF_FOOTER_SIZE);
  sf_free_header* head = (sf_free_header*)GET_HEAD(footer);
  
  cr_assert(footer->block_size == head->header.block_size, "Block sizes do not match!");
  cr_assert(((sf_footer*)((char*)head + PAGE_SIZE - SF_FOOTER_SIZE)) == footer, "Footer location incorrect!");

  sf_footer* footer2 = GET_FOOT(head);
  cr_assert(footer == footer2, "Footer function incorrect!");
}


Test(sf_memsuite, Head_footer_defines, .init = Mem_init, .fini = Mem_fini) {
  sf_footer* footer = (sf_footer*)((char*)sbrk(0) - SF_FOOTER_SIZE);
  sf_header* header = GET_HEAD(footer);
  sf_footer* footer2 = GET_FOOT(header);
  sf_header* header2 = GET_HEAD(footer2);
  cr_assert(header == header2, "GET_HEAD define incorrect!");
  cr_assert(footer == footer2, "GET_FOOT define incorrect!");
}

Test(sf_memsuite, Malloc_an_Integer, .init = Mem_init, .fini = Mem_fini) {
  int *x = Malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Allocate_three_pages, .init = Mem_init, .fini = Mem_fini) {
  sf_header* header = (Malloc((PAGE_SIZE*3) - SF_FOOTER_SIZE - SF_HEADER_SIZE));
  header = header - SF_HEADER_SIZE;
  cr_assert((header->block_size<<4) == ((PAGE_SIZE*3)), "Three pages not allocated!!\n");
}
