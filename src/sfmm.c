#include "sfmm.h"
#include <math.h>
#include <unistd.h>

#define GET_HEAD(foot)				((sf_header*)((char*)((sf_footer*)foot) - (((sf_footer*)foot)->block_size << 4) + SF_FOOTER_SIZE))
#define GET_FOOT(head)				((sf_footer*)((char*)((sf_header*)head) + (((sf_header*)head)->block_size << 4) - SF_FOOTER_SIZE))
#define PREV_BLOCK(node)			((sf_header*)((char*)((sf_footer*)((char*)node - SF_FOOTER_SIZE)) - (((sf_footer*)((char*)node - SF_FOOTER_SIZE))->block_size << 4) + SF_FOOTER_SIZE))
#define NEXT_BLOCK(node)			((sf_header*)((char*)((sf_header*)node) + (((sf_header*)(char*)node)->block_size << 4)))

#define AVAILABLE_SIZE(node, size)	((node->header.block_size << 4) - SF_HEADER_SIZE - SF_FOOTER_SIZE - size)

#define ALIGN(size) 	(size - size%16 + 16)

sf_free_header* freelist_head;
bool initialized = false;
void* heap_start;
void* heap_end;

void Mem_init() {
	if(initialized) {
		warn("%s", "Already initialized, doing nothing");
		return;
	}
	debug("%s","Initializing memory...");
	// Ask for a new page of memory (default: 4096 bytes)

	heap_start = sbrk(PAGE_SIZE);
	if(heap_start == (void *) -1) {
		error("%s", "Could not initialize memory");
		return;
	}
	// The heap_end matches the return pointer if we call sbrk(0)
	heap_end = (heap_start + PAGE_SIZE);
	info("%s: %p", "Heap start", heap_start);
	info("%s: %p", "Heap end", heap_end);

	// We will set the freelist head to the entire empty page
	debug("%s", "Setting freelist_head...");
	freelist_head = heap_start;
	freelist_head->next = NULL;
	freelist_head->prev = NULL;
	
	freelist_head->header.alloc = false;
	freelist_head->header.splinter = false;
	freelist_head->header.block_size = PAGE_SIZE >> 4;

	sf_footer* freelist_foot = GET_FOOT(freelist_head);

	freelist_foot->alloc = freelist_head->header.alloc;
	freelist_foot->splinter = freelist_head->header.splinter;
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
	int requested_size = size;
	if(size%16 != 0) {
		warn("%s", "Not Word Aligned, Aligning...");
		size = ALIGN(size);
	}

	// splitting free block

	sf_free_header* best_fit_head = searchFreeList(size);
	sf_footer* best_fit_foot = GET_FOOT(best_fit_head);

	// If block size is equal, we don't have to split
	if(AVAILABLE_SIZE(best_fit_head, size) == 0) {
		debug("%s", "Block Size is equal to adjusted requested size, split not required");

		// ----------------------------

		// If there is no previous pointer, nor next pointer, freelist_head is not null
		if(best_fit_head->prev == NULL && best_fit_head->next == NULL) {
			warn("%s", "Freelist now empty");
			freelist_head = NULL;
		}
		// If there is a previous pointer but no next pointer
		else if(best_fit_head->prev != NULL && best_fit_head->next == NULL) {
			debug("%s", "Previous pointer is not NULL, setting its next to NULL");
			best_fit_head->prev->next = NULL;
		}
		// If there is no previous pointer but there is a next pointer
		else if(best_fit_head->prev == NULL && best_fit_head->next != NULL) {
			debug("%s", "Previous pointer is NULL, setting next pointer to HEAD");
			best_fit_head->next->prev = NULL;
			freelist_head = best_fit_head->next;
		}
		// Both prev and next not NULL
		else  {
			best_fit_head->next->prev = best_fit_head->prev;
			best_fit_head->prev->next = best_fit_head->next;
		}
		
		best_fit_head->next = NULL;
		best_fit_head->prev = NULL;

		sf_header* new_block_head = (sf_header*)best_fit_head;

		new_block_head->requested_size = requested_size >> 4;
		new_block_head->padding_size = (size - requested_size) >> 4;
		new_block_head->alloc = true;
		new_block_head->splinter = false;
		new_block_head->splinter_size = 0;

		sf_footer* new_block_foot = best_fit_foot;
		new_block_foot->alloc = new_block_head->alloc;
		new_block_foot->block_size = new_block_head->block_size;
		new_block_foot->splinter = new_block_head->splinter;
		
		void* payload = new_block_head + SF_HEADER_SIZE;
		return payload;
	}
	// CHECK FOR SPLINTER HERE

	//-------------------------

	debug("%s", "---------------------------------------");
	debug("%s", "Malloc Block info");
	debug("%s", "---------------------------------------");
	info("%s: %d", "Requested size", requested_size);
	info("%s: %zu", "Block size", size);
	debug("%s", "---------------------------------------");

	// change block size in foot and then retreive the head
	best_fit_foot->block_size = (((best_fit_foot->block_size << 4) - size) >> 4);
	sf_free_header* new_free_head = (sf_free_header*)GET_HEAD(best_fit_foot);
	
	debug("%s", "---------------------------------------");
	debug("%s", "Free Block info");
	debug("%s", "---------------------------------------");
	info("%s: %d", "New Free Block size", best_fit_foot->block_size << 4);
	debug("%s", "---------------------------------------");

	// Transfer free block info to new block
	new_free_head->next = best_fit_head->next;
	new_free_head->prev = best_fit_head->prev;
	new_free_head->header.block_size = best_fit_foot->block_size;
	new_free_head->header.alloc = best_fit_foot->alloc;
	new_free_head->header.splinter = best_fit_foot->splinter;

	// Malloc the new block
	sf_header* new_block_head = (sf_header*)best_fit_head;

	new_block_head->block_size = size >> 4;
	new_block_head->requested_size = requested_size >> 4;
	new_block_head->padding_size = (size - requested_size) >> 4;
	new_block_head->alloc = true;
	new_block_head->splinter = false;
	new_block_head->splinter_size = 0;


	sf_footer* new_block_foot = GET_FOOT(new_block_head);
	new_block_foot->alloc = new_block_head->alloc;
	new_block_foot->block_size = new_block_head->block_size;
	new_block_foot->splinter = new_block_head->splinter;
	
	// Check if newly freed block is a splinter

	//

	void* payload = new_block_head + SF_HEADER_SIZE;
	return payload;
}

void *Realloc(void *ptr, size_t size) {
	return NULL;
}

void Free(void* ptr) {

}

void Mem_fini() {
	
}

/* Search the free list
 * If there is only 1 node, it will check the size
 * If the size is not enough, it will add a new page and coalesce the new page
 * It will continue to do so until there is enough space
 */
sf_free_header* searchFreeList(size_t size) {
	sf_free_header* best_fit;
	// If fit doesn't exist, add a page and try again
	while(!(best_fit = hasFit(size))) {
		addNewPage();
		warn("%s", "Fit not found! Allocating new page!");
	}
	success("%s: %d", "Best fit found of size", best_fit->header.block_size << 4);
	return best_fit;
}

void addNewPage() {
	// Add a new page
	void* new_page_start = sbrk(PAGE_SIZE);
	info("%s: %p", "New Page Pointer", new_page_start);

	// Insert new freelist node at head for O(1) efficiency
	debug("%s", "Setting new freelist node...");
	sf_free_header* new = new_page_start;
	freelist_head->prev = new;
	new->next = freelist_head;
	
	new->header.alloc = false;
	new->header.splinter = false;
	new->header.block_size = PAGE_SIZE >> 4;

	sf_footer* new_foot = GET_FOOT(new);
	heap_end = (char*)new_foot + SF_FOOTER_SIZE;
	debug("%s: %p", "New heap end", heap_end);

	new_foot->alloc = new->header.alloc;
	new_foot->splinter = new->header.splinter;
	new_foot->block_size = new->header.block_size;

	info("%s: %p", "Freelist Node New Page Address", new);
	info("%s: %p", "Freelist Node New Page Foot Address", new_foot);
	info("%s: %d", "Block Size", new->header.block_size << 4);

	freelist_head = new;

	// Coalesce with memory directly behind new block
	coalesce(new_page_start);
}

bool blockValid(sf_header* head) {
	debug("%s", "Validating block");
	sf_footer* foot = GET_FOOT(head);
	if((head->alloc == foot->alloc) && (head->block_size == foot->block_size) && (head->splinter == foot->splinter)) {
		success("%s", "Block is valid");
		return true;
	}
	warn("%s", "Invalid block detected");
	return false;
}

void coalesce(sf_header* node) {
	// Get all free blocks immediately following current memory but make sure we don't go to heap end, we will coalesce all of them
	sf_header* next_block = NEXT_BLOCK(node);
	if(next_block != heap_end) {
		while(blockValid(next_block) && !(next_block->alloc)) {
			node = next_block;
			next_block = NEXT_BLOCK(next_block);
		}
	}

	// Get the block header and footer that is directly behind in the memory
	sf_header* possible_free_head = PREV_BLOCK(node);
	sf_footer* possible_free_foot = GET_FOOT(possible_free_head);
	info("%s: %p", "Possible Free Head", possible_free_head);
	info("%s: %p", "Possible Free Foot", possible_free_foot);
	// If block is invalid, something is wrong elsewhere
	if(!blockValid(possible_free_head)) {
		error("%s", "Invalid block, aborting");
		return;
	}
	// If block is not free, do nothing
	if(possible_free_head->alloc) {
		debug("%s", "Block is allocated, no coalesce required");
		return;
	}
	// Coalescing here
	debug("%s", "Coalescing...");
	sf_free_header* new_free_head = (sf_free_header*)possible_free_head;
	sf_footer* new_free_foot = GET_FOOT(node);
	info("%s: %p", "Coalesced Free Header: ", new_free_head);
	info("%s: %p", "Coalesced Free Footer: ", new_free_foot);
	new_free_head->header.block_size += node->block_size;
	new_free_foot->block_size = new_free_head->header.block_size;
	info("%s: %d", "Header Block Size: ", new_free_head->header.block_size << 4);
	info("%s: %d", "Footer Block Size: ", new_free_foot->block_size << 4);

	// Cleanup next and prev pointers
	debug("%s", "Cleaning up pointers...");
	sf_free_header* start = freelist_head;
	while(start != NULL) {
		// If next address equals the block we just removed, set it to the new one
		if(start->next == (sf_free_header*)node) {
			start->next = new_free_head;
			// If next address is the same, we can safely NULL
			if(start == start->next) {
				info("%s", "Next pointer same as current pointer, setting next to null");
				start->next = NULL;
			}
		}
		// If prev address equals the block we just removed, set it to the new one
		if(start->prev == (sf_free_header*)node) {
			start->prev = new_free_head;
			// If next address is the same, we can safely NULL
			// This also means that this address is the new head
			if(start == start->prev) {
				info("%s", "Prev pointer same as current pointer, setting prev to null and current to freelist head");
				start->prev = NULL;
				freelist_head = start;
			}
		}
		start = start->next;
	}
	// If not beginning of heap, do it again until no more
	if(!((void*)new_free_head == heap_start)) {
		coalesce((sf_header*)new_free_head);
		return;
	}
}

sf_free_header* hasFit(size_t size) {
	sf_free_header* search = freelist_head;
	sf_free_header* best_fit = NULL;
	int best_size = -1;
	while(search != NULL) {
		int available_size = AVAILABLE_SIZE(search, size);
		info("%s: %d", "Available size", available_size);
	
		// If block is large enough and we don't get a best fit yet, add it
		if(available_size >= 0 && best_fit == NULL) {
			info("%s", "best_fit is NULL but found a fit");
			best_fit = search;
			best_size = available_size;
		}
		// If block is better fit than our already fitted block, replace
		else if(available_size >= 0 && available_size < best_size) {
			info("%s", "best_fit already exists but found a better fit");
			best_fit = search;
			best_size = available_size;
		}
		search = search->next;
	}
	return best_fit;
}