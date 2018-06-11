#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"

#define GET_HEAD(foot)				((sf_header*)((char*)((sf_footer*)foot) - (((sf_footer*)foot)->block_size << 4) + SF_FOOTER_SIZE))
#define GET_FOOT(head)				((sf_footer*)((char*)((sf_header*)head) + (((sf_header*)head)->block_size << 4) - SF_FOOTER_SIZE))
#define PREV_BLOCK(node)			((sf_header*)((char*)((sf_footer*)((char*)node - SF_FOOTER_SIZE)) - (((sf_footer*)((char*)node - SF_FOOTER_SIZE))->block_size << 4) + SF_FOOTER_SIZE))
#define NEXT_BLOCK(node)			((sf_header*)((char*)((sf_header*)node) + (((sf_header*)(char*)node)->block_size << 4)))

Test(sf_memsuite, Init_free_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  sf_footer* footer = (sf_footer*)((char*)sbrk(0) - SF_FOOTER_SIZE);
  sf_free_header* head = (sf_free_header*)GET_HEAD(footer);
  
  cr_assert(footer->block_size == head->header.block_size, "Block sizes do not match!");
  cr_assert(((sf_footer*)((char*)head + PAGE_SIZE - SF_FOOTER_SIZE)) == footer, "Footer location incorrect!");

  sf_footer* footer2 = GET_FOOT(head);
  cr_assert(footer == footer2, "Footer function incorrect!");
}

Test(sf_memsuite, Head_footer_defines, .init = sf_mem_init, .fini = sf_mem_fini) {
  sf_footer* footer = (sf_footer*)((char*)sbrk(0) - SF_FOOTER_SIZE);
  sf_header* header = GET_HEAD(footer);
  sf_footer* footer2 = GET_FOOT(header);
  sf_header* header2 = GET_HEAD(footer2);
  cr_assert(header == header2, "GET_HEAD define incorrect!");
  cr_assert(footer == footer2, "GET_FOOT define incorrect!");
}

Test(sf_memsuite, sf_malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int* x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Allocate_three_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
  sf_header* header = (sf_malloc((PAGE_SIZE*3) - SF_FOOTER_SIZE - SF_HEADER_SIZE));
  header = (sf_header*)((char*)header - SF_HEADER_SIZE);
  cr_assert((header->block_size<<4) == ((PAGE_SIZE*3)), "Three pages not allocated!!\n");
}

Test(sf_memsuite, sf_free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void* pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - SF_HEADER_SIZE;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer + (sfHeader->block_size << 4) - SF_FOOTER_SIZE);
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int* x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int* x = sf_malloc(4);
  int* y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header* sfHeader = (sf_header *)((char*)x - SF_HEADER_SIZE);
  sfHeader = NEXT_BLOCK(sfHeader);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert((sfHeader->splinter_size << 4) == 16, "Splinter size is not 16");
  sf_footer* sfFooter = GET_FOOT(sfHeader);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in the footer is not 1!");
}
