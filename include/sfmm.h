#ifndef SFMM_H
#define SFMM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "debug.h"

#define PADDING_SIZE_BITS 4
#define SPLINTER_SIZE_BITS 5
#define UNUSED_SIZE_BITS 9
#define REQUEST_SIZE_BITS 14
#define BLOCK_SIZE_BITS 28
#define SPLINTER_CONSUMED_BITS 2
#define ALLOC_SIZE_BITS 2

#define PAGE_SIZE 4096

#define SF_HEADER_SIZE \
    ((ALLOC_SIZE_BITS + BLOCK_SIZE_BITS + UNUSED_SIZE_BITS + \
      PADDING_SIZE_BITS + REQUEST_SIZE_BITS + SPLINTER_SIZE_BITS + SPLINTER_CONSUMED_BITS) >> 3)

#define SF_FOOTER_SIZE SF_HEADER_SIZE

struct __attribute__((__packed__)) sf_header {
  uint64_t          alloc : ALLOC_SIZE_BITS;
  uint64_t       splinter : SPLINTER_CONSUMED_BITS;
  uint64_t     block_size : BLOCK_SIZE_BITS;
  uint64_t requested_size : REQUEST_SIZE_BITS;
  uint64_t    unused_bits : UNUSED_SIZE_BITS;
  uint64_t  splinter_size : SPLINTER_SIZE_BITS;
  uint64_t   padding_size : PADDING_SIZE_BITS;

};

typedef struct sf_header sf_header;

struct __attribute__((__packed__)) sf_free_header {
    sf_header header;
    struct sf_free_header *next;
    struct sf_free_header *prev;
};

typedef struct sf_free_header sf_free_header;

struct __attribute__((__packed__)) sf_footer {
    uint64_t      alloc : ALLOC_SIZE_BITS;
    uint64_t   splinter : SPLINTER_CONSUMED_BITS;
    uint64_t block_size : BLOCK_SIZE_BITS;
};

typedef struct sf_footer sf_footer;

extern sf_free_header *freelist_head;

void Mem_init();
void Mem_fini();

void *Malloc(size_t size);
void *Realloc(void *ptr, size_t size);
void Free(void *ptr);

sf_free_header* searchFreeList(size_t size);
void addNewPage();
bool blockValid(sf_header* head, sf_footer* foot);
void coalesceBack(sf_header* node);
sf_free_header* hasFit(size_t size);

#endif
