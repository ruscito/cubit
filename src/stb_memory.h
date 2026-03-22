// stb_memory.h

#ifndef STB_MEMORY_H_
#define STB_MEMORY_H_

/* TODO:
 * mmap is and MAP_ANON | MAP_PRIVATE is POSIX so it will fail on a
 * window system. Need to swap it for VirtualAlloc if need to support
 * window */

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_ALIGNMENT 16

// Data type definition
typedef struct {
	size_t size;	// Arena size
	size_t offset;	// Offset for the non allocated area
	void *data;		// The pointer to the arena
}memory_t ;


/*
 * Create the arena allocator by the size possed
 */
void arena_create(memory_t *arena, size_t size);

/*
 * Allocate memory from the arena. It gets the arena and
 * the size of the type passed
 */
void *arena_alloc(memory_t *arena, size_t size);

/*
 * Destroy the arena. Normally should be used when there is
 * a need to release the memoy.
 */
void arena_destroy(memory_t *arena);

/*
 * Reset arena offset. It like a soft reinitialize of
 * the aren without release the memory
 */
void arena_reset(memory_t *arena);

#ifdef STB_MEMORY_IMPLEMENTATION

void arena_create(memory_t *arena, size_t size) {
	arena->offset = 0;
	arena->size = size;
	arena->data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (arena->data == MAP_FAILED) {
		perror("arena init");
		exit(-1);
	}

#ifdef DEBUG
	printf("arena->size:%zu\n", arena->size);
#endif
}

void *arena_alloc(memory_t *arena, size_t size) {
	size_t aligned_offset = (arena->offset + (DEFAULT_ALIGNMENT - 1)) & ~(DEFAULT_ALIGNMENT - 1);

	if (size > (arena->size - aligned_offset)) {
		perror("No room");
		return NULL;
	}
	void *retval = (char*) arena->data + aligned_offset;
	arena->offset = aligned_offset + size;

#ifdef DEBUG
	printf("arena->offset:%zu\n", arena->offset);
	printf("arena available:%zu\n", (arena->size - arena->offset));
#endif

	return retval;
}

void arena_destroy(memory_t *arena) {
	if (munmap(arena->data, arena->size) == -1) perror("arena_destroy");
	arena->size = 0;
	arena->offset = 0;
	arena->data = NULL;
}

void arena_reset(memory_t *arena) {
	arena->offset = 0;
}

#endif // STB_MEMORY_IMPLEMETATION
#endif // STB_MEMORY_H_
