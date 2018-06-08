#include "sfmm.h"
#include <math.h>
#include <unistd.h>

#define GET_FREE_HEAD(foot)	((sf_free_header*)((char*)foot - (foot->block_size << 4) + SF_FOOTER_SIZE))
#define GET_FREE_FOOT(node)	((sf_footer*)((char*)node + (node->header.block_size << 4) - SF_FOOTER_SIZE))

#define QUAD_ALIGN(size) 	(size - size%16 + 16)

sf_free_header* freelist_head;
bool initialized = false;

void Mem_init() {
	if(initialized) {
		warn("%s", "Already initialized, doing nothing");
		return;
	}
	debug("%s","Initializing memory for first time...");
	// Ask for a new page of memory (default: 4096 bytes)
	// If not Quad-Word aligned, then align
	// This case should NOT happen, unless you set the PAGE_SIZE to a non-quad word alignment
	size_t size = PAGE_SIZE;
	if(size%16 != 0) {
		warn("%s", "Not Quad Word Aligned, Aligning...");
		size = QUAD_ALIGN(size);
		info("%s: %zu", "New Free Node Size", size);
	}

	void *heap_start = sbrk(size);
	if(heap_start == (void *) -1) {
		error("%s", "Could not initialize memory");
		return;
	}
	// The heap_end matches the return pointer if we call sbrk(0)
	void *heap_end = (heap_start + size);
	info("%s: %p", "Heap start", heap_start);
	info("%s: %p", "Heap end", heap_end);

	// We will set the freelist head to the entire empty page
	debug("%s", "Setting freelist_head...");
	freelist_head = heap_start;
	freelist_head->next = NULL;
	freelist_head->prev = NULL;
	
	freelist_head->header.alloc = false;
	freelist_head->header.block_size = size >> 4;

	sf_footer* freelist_foot = GET_FREE_FOOT(freelist_head);

	freelist_foot->alloc = freelist_head->header.alloc;
	freelist_foot->block_size = freelist_head->header.block_size;

	info("%s: %p", "Freelist Head address", freelist_head);
	info("%s: %p", "Foot address", freelist_foot);
	info("%s: %d", "Block Size", freelist_head->header.block_size << 4);
	
	initialized = true;
}

void *Malloc(size_t size) {
	if(size <= 0) {
		error("%s", "Invalid size");
		return NULL;
	}
	if(!initialized) {
		warn("%s", "Memory not initialized, calling Mem_init()");
		Mem_init();
	}
	info("%s: %zu", "Requested size", size);

	return NULL;
}

void *Realloc(void *ptr, size_t size) {
	return NULL;
}

void Free(void* ptr) {

}

void Mem_fini() {
	
}

void setFreeNode(sf_free_header* node, size_t size) {
/*	debug("%s", "Setting Free Node...");
	info("%s: %zu", "Free Node Size", size);

	node->header.alloc = false;
	//node->header.block_size = size >> 4;
	// We don't need to change these values for the freelist
	//node->header.requested_size = 0;
	//node->header.splinter_size = 0;
	//node->header.padding_size = 0;
	//node->header.splinter = false;

	sf_footer* node_foot = GET_FREE_FOOT(node);
	
	node_foot->alloc = node->header.alloc;
	//node_foot->block_size = node->header.block_size;
	// We don't need to change these values for the freelist
	//node_foot->alloc = node->header.alloc;
	//node_foot->splinter = node->header.splinter;

	info("%s: %p", "Node address", node);
	info("%s: %p", "Foot address", node_foot);
	//info("%s: %d", "Block Size", node->header.block_size << 4);
	// We don't need to change these values for the freelist
	//info("%s: %d", "Splinter", node->header.splinter);
	//info("%s: %d", "Splinter Size", node->header.splinter_size);
	//info("%s: %d", "Padding Size", node->header.padding_size);*/
}
