// batch.c

#include "batch.h"
#include "light.h"
#include "stb_memory.h"

#include <stdlib.h>
#include <stdbool.h>

#define MAX_ENTRY 		100
#define MAX_TRANSFORMS 	10000

static struct {
	batch_registry_entry_t *entry;
	uint32_t count;
	uint32_t objects_count;
} batch_registry;

static memory_t arena;



/*
 * Add a transform to the i position of the batch entry's transforms array.
 */
static void add_transform(mat4 transform, uint32_t i) {
	if (batch_registry.entry[i].count >= MAX_TRANSFORMS-1) {
		/* TODO:
		 * need to decide what to do in this case. Maybe save the current information
		 * flush out the BR forcing the renderer to draw and now with BR cleaned up
		 * add back the saved transform. For now just return */
		fprintf(stderr, "Could not add transform to the Batch Regitry\n");
		return;
	}
	batch_registry.entry[i].transforms[batch_registry.entry[i].count] = transform;
	batch_registry.entry[i].count ++;
	batch_registry.objects_count ++;
#ifdef DEBUG_FRUSTUM
	printf("Total object count %d\n", batch_registry.objects_count);
#endif
}

/*
 * Create a new entry in the BR and allocate memory for it
 */
static void add_new_entry(mesh_t* mesh, material_t* material, mat4 transform, mat4 vp) {
	if (batch_registry.count >= MAX_ENTRY-1) {
		/* TODO:
		 * need to decide what to do in this case. Maybe save the current information
		 * flush out the BR forcing the renderer to draw and now with BR cleaned up
		 * add back the saved transform. For now just return */
		fprintf(stderr, "Could not add a new entry to the Batch Regitry\n");
		return;
	}
	batch_registry.entry[batch_registry.count].transforms = arena_alloc(&arena, sizeof(mat4)*MAX_TRANSFORMS);
	batch_registry.entry[batch_registry.count].mesh = mesh;
	batch_registry.entry[batch_registry.count].material = material;
	batch_registry.entry[batch_registry.count].vp = vp;
	batch_registry.entry[batch_registry.count].count = 0;
	add_transform(transform, batch_registry.count);
	batch_registry.count ++;
}

void batch_init(void) {
	arena_create(&arena,
			sizeof(batch_registry_entry_t)*MAX_ENTRY +
			sizeof(mat4)*MAX_TRANSFORMS*MAX_ENTRY);
	batch_registry.entry = arena_alloc(&arena, sizeof(batch_registry_entry_t)*MAX_ENTRY);
	batch_registry.count = 0;
	batch_registry.objects_count = 0;
}

void batch_cleanup(void) {
	arena_reset(&arena);
	batch_registry.entry = arena_alloc(&arena, sizeof(batch_registry_entry_t)*MAX_ENTRY);
	batch_registry.count = 0;
	batch_registry.objects_count = 0;
}

void batch_shutdown(void) {
	arena_destroy(&arena);
}

void batch_flush(void) {
	;
}

void batch_push(mesh_t* mesh, material_t* material, mat4 transform, mat4 pv) {
	if (batch_registry.count == 0) {
		add_new_entry(mesh, material, transform, pv);
		return;
	}

	for (uint32_t i = 0; i < batch_registry.count; i++) {
		if (material == batch_registry.entry[i].material
				&& mesh == batch_registry.entry[i].mesh) {
			add_transform(transform, i);
			return;
		}
	}
	add_new_entry(mesh, material, transform, pv);
}

uint32_t  batch_size(void) {
	return batch_registry.count;
}


batch_registry_entry_t batch_get_entry(uint32_t index) {
	return batch_registry.entry[index];
}
