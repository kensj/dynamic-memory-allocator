#include "sfmm.h"
#include <math.h>
#include <unistd.h>
#include <string.h>

#define GET_HEAD(foot)				((sf_header*)((char*)((sf_footer*)foot) - (((sf_footer*)foot)->block_size << 4) + SF_FOOTER_SIZE))
#define GET_FOOT(head)				((sf_footer*)((char*)((sf_header*)head) + (((sf_header*)head)->block_size << 4) - SF_FOOTER_SIZE))
#define PREV_BLOCK(node)			((sf_header*)((char*)((sf_footer*)((char*)node - SF_FOOTER_SIZE)) - (((sf_footer*)((char*)node - SF_FOOTER_SIZE))->block_size << 4) + SF_FOOTER_SIZE))
#define NEXT_BLOCK(node)			((sf_header*)((char*)((sf_header*)node) + (((sf_header*)(char*)node)->block_size << 4)))

#define AVAILABLE_SIZE(node, size)	((node->header.block_size << 4) - size)

//#define ALIGN(size) 	(size - size%16 + 16)

sf_free_header* freelist_head;
bool initialized = false;
void* heap_start;
void* heap_end;

void sf_mem_init() {
	BREAK("MEMORY INITIALIZATION");

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
	heap_end = ((char*)heap_start + PAGE_SIZE);
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
	info("%s: %p", "Freelist Head Next", freelist_head->next);
	info("%s: %p", "Freelist Head Prev", freelist_head->prev);
	info("%s: %d", "Block Size", freelist_head->header.block_size << 4);

	initialized = true;

	END_BREAK("MEMORY INITIALIZATION");
}

void* sf_malloc(size_t size) {
	BREAK("MALLOC");

	if(size <= 0) {
		error("%s", "Invalid size");

		END_BREAK("MALLOC");
		return NULL;
	}
	if(!initialized) {
		warn("%s", "Memory not initialized, calling sf_mem_init()");
		sf_mem_init();
	}
	int requested_size = size;
	size += SF_HEADER_SIZE + SF_FOOTER_SIZE;
	if(size%(SF_HEADER_SIZE + SF_FOOTER_SIZE) != 0) {
		warn("%s", "Not Word Aligned, Aligning...");
		size = (size - size%16 + 16);
	}

	// Get the best fit
	sf_free_header* best_fit_head = searchFreeList(size);	
	if(best_fit_head == (void *) -1) {
		error("%s", "Could not allocate new page!");
		END_BREAK("MALLOC");
		return NULL;
	}
	
	sf_footer* best_fit_foot = GET_FOOT(best_fit_head);
	info("%s: %d", "Requested size", requested_size);
	info("%s: %zu", "Block size", size);
	
	// If block size is equal, we don't have to split
	if(AVAILABLE_SIZE(best_fit_head, size) == 0) {
		debug("%s", "Free block Size is exact, split not required");
		
		// Remove this node from freelist as we will allocate it now
		replaceFreeListPointers(freelist_head, best_fit_head, NULL);

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
		
		void* payload = (char*)new_block_head + SF_HEADER_SIZE;
		
		success("%s: %p", "Successfully placed, header location", new_block_head);
		success("%s: %p", "Successfully placed, footer location", new_block_foot);
		info("%s: %p", "Payload location", payload);

		END_BREAK("MALLOC");
		return payload;
	}

	// Block size is not equal, so now we must split the block
	debug("%s", "Block size is not equal, so we must split the block");

	// Get the new freelist node
	sf_free_header* new_free_head = (sf_free_header*)((char*)best_fit_head + size);
	// Copy old freelist info to new block and subtract block size
	new_free_head->header.alloc = false;
	new_free_head->header.block_size = ((best_fit_head->header.block_size << 4) - size) >> 4;
	new_free_head->next = best_fit_head->next;
	new_free_head->prev = best_fit_head->prev;
	// Set the footer
	sf_footer* new_free_foot = GET_FOOT(new_free_head);
	new_free_foot->alloc = new_free_head->header.alloc;
	new_free_foot->block_size = new_free_head->header.block_size;
	// Check if it is a splinter
	if((new_free_head->header.block_size << 4) <= (SF_HEADER_SIZE + SF_FOOTER_SIZE)) {
		new_free_head->header.splinter = true;
		new_free_head->header.splinter_size = new_free_head->header.block_size;
	}
	new_free_foot->splinter = new_free_head->header.splinter;

	info("%s: %p", "New Free Block Header", new_free_head);
	info("%s: %d", "New Free Block Size", (new_free_head->header.block_size << 4));
	info("%s: %d", "Splinter Size", (new_free_head->header.splinter_size << 4));
	info("%s: %d", "Is Splinter", new_free_head->header.splinter);

	// Set the allocated block now
	sf_header* new_block_head = (sf_header*)best_fit_head;
	new_block_head->block_size = size >> 4;
	new_block_head->requested_size = requested_size >> 4;
	new_block_head->padding_size = (size - requested_size) >> 4;
	new_block_head->alloc = true;
	new_block_head->splinter = false;
	// Set the footer
	sf_footer* new_block_foot = GET_FOOT(new_block_head);
	new_block_foot->alloc = new_block_head->alloc;
	new_block_foot->block_size = new_block_head->block_size;
	new_block_foot->splinter = new_block_head->splinter;

	// Payload
	void* payload = (char*)new_block_head + SF_HEADER_SIZE;
	success("%s: %p", "Successfully placed, header location", new_block_head);
	success("%s: %p", "Successfully placed, footer location", new_block_foot);
	info("%s: %p", "Payload location", payload);
	info("%s: %p", "Freelist Head", freelist_head);
	info("%s: %p", "Freelist Head Next", freelist_head->next);
	info("%s: %p", "Freelist Head Prev", freelist_head->prev);

	replaceFreeListPointers(freelist_head, (sf_free_header*)new_block_head, new_free_head);

	END_BREAK("MALLOC");
	return payload;
}

void* sf_realloc(void *ptr, size_t size) {
	BREAK("REALLOC");

	if(size <= 0) {
		error("%s", "Invalid size");

		END_BREAK("MALLOC");
		return NULL;
	}
	if(!initialized) {
		warn("%s", "Memory not initialized, calling sf_mem_init()");
		sf_mem_init();
	}
	int requested_size = size;
	size += SF_HEADER_SIZE + SF_FOOTER_SIZE;
	if(size%(SF_HEADER_SIZE + SF_FOOTER_SIZE) != 0) {
		warn("%s", "Not Word Aligned, Aligning...");
		size = (size - size%16 + 16);
	}

	// If pointer doesn't exist, just malloc
	if(ptr == NULL) {
		warn("%s", "Pointer does not exist, mallocing new block");
		return sf_malloc(requested_size);
	}

	sf_header* block_head = (sf_header*)((char*)ptr - SF_HEADER_SIZE);

	// If block is invalid
	if(!blockValid(block_head)) {
		error("%s", "Invalid block!");
		END_BREAK("REALLOC");
		return NULL;
	}
	
	int available_size = (block_head->block_size << 4) - size;
	info("%s: %d", "Available Size", available_size);

	// If block size is the same, we don't hae to do anything
	if(available_size == 0) {
		success("%s", "No need to reallocate");
		END_BREAK("REALLOC");
		return ptr;
	}

	// Otherwise either find a new block (if not enough space), or split block (less space)
	if(available_size < 0) {
		info("%s", "We must find a new block");
		// Malloc into a new block
		void* new_payload = sf_malloc(requested_size);
		// Copy the data
		memcpy(new_payload, ptr, requested_size);
		// Then free the old block
		sf_free(ptr);
		// Our other functions handle coalescing, realloc is straightforward now :-)

		END_BREAK("REALLOC");
		return new_payload;
	}
	else {
		info("%s", "We must split this block");
		// Set the block first
		block_head->block_size = size >> 4;
		sf_footer* block_foot = GET_FOOT(block_head);
		block_foot->alloc = block_head->alloc;
		block_foot->splinter = block_head->splinter;
		block_foot->block_size = block_head->block_size;

		// set free list
		sf_free_header* new_free_head = (sf_free_header*)NEXT_BLOCK(block_head);
		new_free_head->header.alloc = false;
		new_free_head->header.block_size = available_size >> 4;
		// Set the footer
		sf_footer* new_free_foot = GET_FOOT(new_free_head);
		new_free_foot->alloc = new_free_head->header.alloc;
		new_free_foot->block_size = new_free_head->header.block_size;
		// Check if it is a splinter
		if((new_free_head->header.block_size << 4) <= (SF_HEADER_SIZE + SF_FOOTER_SIZE)) {
			new_free_head->header.splinter = true;
			new_free_head->header.splinter_size = new_free_head->header.block_size;
		}
		new_free_foot->splinter = new_free_head->header.splinter;

		info("%s: %p", "New Free Block Header", new_free_head);
		info("%s: %d", "New Free Block Size", (new_free_head->header.block_size << 4));
		info("%s: %d", "Splinter Size", (new_free_head->header.splinter_size << 4));
		info("%s: %d", "Is Splinter", new_free_head->header.splinter);
		// Insert to beginning of free list
		freelist_head->prev = new_free_head;
		new_free_head->next = freelist_head;
		freelist_head = new_free_head;
		coalesce(freelist_head);

		END_BREAK("REALLOC");
		return (void*)((char*)block_head + SF_HEADER_SIZE);
	}
}

void sf_free(void* ptr) {
	BREAK("FREE");
	
	sf_header* header = (sf_header*)((char*)ptr - SF_HEADER_SIZE);
	sf_footer* footer = (sf_footer*)GET_FOOT(header);
	
	if(!blockValid(header)) {
		error("%s", "Invalid block!");
		END_BREAK("FREE");
		return;
	}

	info("%s: %p", "Payload Location", ptr);
	info("%s: %p", "Header Location", header);
	info("%s: %p", "Footer Location", footer);
	info("%s: %d", "Block Size", header->block_size << 4);

	header->alloc = false;
	footer->alloc = false;

	// Insert to beginning of freelist
	debug("%s", "Inserting Free Node to beginning to list");
	freelist_head->prev = (sf_free_header*)header;
	freelist_head->prev->next = freelist_head;
	freelist_head = freelist_head->prev;
	info("%s: %p", "Freelist Head", freelist_head);
	info("%s: %p", "Freelist Head Next", freelist_head->next);
	info("%s: %p", "Freelist Head Prev", freelist_head->prev);

	coalesce(freelist_head);
	END_BREAK("FREE");
}

void sf_mem_fini() {
	
}

/* Search the free list
 * If there is only 1 node, it will check the size
 * If the size is not enough, it will add a new page and coalesce the new page
 * It will continue to do so until there is enough space
 */
sf_free_header* searchFreeList(size_t size) {
	BREAK("SEARCH FREE LIST");

	sf_free_header* best_fit;
	// If fit doesn't exist, add a page and try again
	while(!(best_fit = hasFit(size))) {
		if(addNewPage() < 0) {
			error("%s", "Could not allocate new page!");

			END_BREAK("SEARCH FREE LIST");
			return (void*)-1;
		}
		warn("%s", "Fit not found! Allocating new page!");
	}
	success("%s: %d", "Best Fit", best_fit->header.block_size << 4);

	END_BREAK("SEARCH FREE LIST");
	return best_fit;
}

sf_free_header* hasFit(size_t size) {
	BREAK("HAS FIT");

	sf_free_header* search = freelist_head;
	sf_free_header* best_fit = NULL;
	int best_size = -1;
	while(search != NULL) {
		int available_size = AVAILABLE_SIZE(search, size);
		info("%s: %d", "Available Size", available_size);
	
		// If block is large enough and we don't get a best fit yet, add it
		if(available_size >= 0 && best_fit == NULL) {
			info("%s", "best_fit is NULL but found a potential fit");
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

	END_BREAK("HAS FIT");
	return best_fit;
}

int addNewPage() {
	BREAK("ADD NEW PAGE");

	// Add a new page
	void* new_page_start = sbrk(PAGE_SIZE);
	if(new_page_start == (void *) -1) {
		error("%s", "Could not initialize memory");

		END_BREAK("ADD NEW PAGE");
		return -1;
	}

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

	new_foot->alloc = new->header.alloc;
	new_foot->splinter = new->header.splinter;
	new_foot->block_size = new->header.block_size;


	info("%s: %p", "Freelist Node New Page Address", new);
	info("%s: %p", "Freelist Node New Page Foot Address", new_foot);
	info("%s: %d", "Block Size", new->header.block_size << 4);

	freelist_head = new;

	heap_end = (char*)new_foot + SF_FOOTER_SIZE;
	info("%s: %p", "New heap end", heap_end);

	// Coalesce with memory on both sides of the new block
	coalesce(new_page_start);

	END_BREAK("ADD NEW PAGE");
	return 1;
}

void coalesce(sf_free_header* node) {
	BREAK("COALESCE");

	if(node != heap_start) node = coalesceBackward(node);
	node = coalesceForward(node);

	END_BREAK("COALESCE");
	return;
}

sf_free_header* coalesceBackward(sf_free_header* node) {
	BREAK("COALESCE BACKLWARD");
	// Get the previous block
	sf_header* previous_block_head = PREV_BLOCK(node);
	sf_footer* previous_block_foot = GET_FOOT(previous_block_head);
	info("%s: %p", "Previous block head", previous_block_head);
	info("%s: %p", "Previous block foot", previous_block_foot);
	info("%s: %p", "Heap start", heap_start);

	// If previous block address is less than the heap start, we do nothing to it
	if((void*)previous_block_head < heap_start) {
		warn("%s","Previous block does not exist, no need to coalesce backwards");
		END_BREAK("COALESCE BACKLWARD");
		return node;
	}
	// Or if it is not a valid block, throw an error. Something is wrong somewhere else
	else if(!blockValid(previous_block_head)) {
		error("%s", "Invalid block!");
		END_BREAK("COALESCE BACKLWARD");
		return node;
	}
	// If the block is not free, we don't have to touch it either
	else if(previous_block_head->alloc) {
		debug("%s", "Block is allocated, no coalesce required");
		END_BREAK("COALESCE BACKLWARD");
		return node;
	}

	// Coalesce backwards here
	debug("%s", "Coalescing backwards");

	sf_free_header* new_free_head = (sf_free_header*)previous_block_head;
	sf_footer* new_free_foot = GET_FOOT(node);

	new_free_head->header.block_size += node->header.block_size;
	new_free_foot->block_size = new_free_head->header.block_size;
	
	info("%s: %p", "New Block Head", new_free_head);
	info("%s: %p", "New Block Foot", new_free_foot);
	info("%s: %d", "New Block Size", new_free_head->header.block_size << 4);

	// Cleanup next & prev pointers of the freelist
	// We replace every instance of node with new_free_head
	replaceFreeListPointers(freelist_head, (sf_free_header*)node, new_free_head);

	// Since we coalesced backwards already, we use the new freelist to check the next node
	node = new_free_head;

	END_BREAK("COALESCE BACKLWARD");
	return node;
}

sf_free_header* coalesceForward(sf_free_header* node) {
	BREAK("COALESCE FORWARD");
	// Get the next block
	sf_header* next_block_head = NEXT_BLOCK(node);
	// If beyond the heap, do nothing
	if((void*)next_block_head >= heap_end) {
		warn("%s","Next block does not exist, no need to coalesce forwards!");
		END_BREAK("COALESCE FORWARD");
		return node;
	}
	sf_footer* next_block_foot = GET_FOOT(next_block_head);
	info("%s: %p", "Next block head", next_block_head);
	info("%s: %p", "Next block foot", next_block_foot);
	info("%s: %p", "Heap end", heap_end);

	// If the next block is invalid, we have a problem in the program
	if(!blockValid(next_block_head)) {
		error("%s", "Invalid block!");
		END_BREAK("COALESCE FORWARD");
		return node;
	}
	// If the block is not free, we don't have to touch it either
	else if(next_block_head->alloc) {
		debug("%s", "Block is allocated, no coalesce required");
		END_BREAK("COALESCE FORWARD");
		return node;
	}
	sf_free_header* new_free_head = (sf_free_header*)node;
	sf_footer* new_free_foot = next_block_foot;

	new_free_head->header.block_size += next_block_head->block_size;
	new_free_foot->block_size = new_free_head->header.block_size;

	info("%s: %p", "New Block Head", new_free_head);
	info("%s: %p", "New Block Foot", new_free_foot);
	info("%s: %d", "New Block Size", new_free_head->header.block_size << 4);

	// This time we remove the next block
	replaceFreeListPointers(freelist_head, (sf_free_header*)next_block_head, new_free_head);

	END_BREAK("COALESCE FORWARD");
	return node;
}

bool blockValid(sf_header* head) {
	BREAK("CHECK IF BLOCK IS VALID");

	debug("%s: %p", "Validating block at address", head);
	if(head == heap_end) {
		warn("%s", "Block is heap end, aborting");

		END_BREAK("CHECK IF BLOCK IS VALID");
		return false;
	}
	sf_footer* foot = GET_FOOT(head);
	if((head->alloc == foot->alloc) && (head->block_size == foot->block_size) && (head->splinter == foot->splinter)) {
		success("%s", "Block is valid");

		END_BREAK("CHECK IF BLOCK IS VALID");
		return true;
	}
	warn("%s", "Invalid block detected");
	
	END_BREAK("CHECK IF BLOCK IS VALID");
	return false;
}

// Recursive function
void replaceFreeListPointers(sf_free_header* start, sf_free_header* node_to_replace, sf_free_header* node_to_insert) {
	BREAK("CLEANUP POINTERS");
	debug("%s", "Cleaning up pointers...");
	info("%s: %p", "Node to replace", node_to_replace);
	info("%s: %p", "Node to insert", node_to_insert);

	if(start == NULL) return;
	else if(start == (sf_free_header*)node_to_replace) {
		info("%s", "Current pointer is the node to replace...");
		if(start->prev != NULL) {
			info("%s", "Previous pointer is not null, replacing previous' next...");
			start->prev->next = node_to_insert;
		}
		else if(start->prev == NULL) {
			info("%s", "Previous pointer is null, setting current pointer to noode to insert");
			freelist_head = node_to_insert;
			freelist_head->next = start->next;
		}
	}
	if(start->next == start) start->next = NULL;
	if(start->next != NULL) replaceFreeListPointers(start->next, node_to_replace, node_to_insert);

	if(freelist_head != NULL) {
		info("%s: %p", "Freelist Head", freelist_head);
		info("%s: %p", "Freelist Foot", GET_FOOT(freelist_head));
		info("%s: %p", "Freelist Head->Next", freelist_head->next);
		info("%s: %p", "Freelist Head->Prev", freelist_head->prev);
	}
	else {
		info("%s", "Freelist is empty");
	}

	END_BREAK("CLEANUP POINTERS");
}
