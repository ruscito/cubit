// batch.c

#include "batch.h"
#include "light.h"
#include "stb_memory.h"

#include <stdlib.h>
#include <stdbool.h>

#define MAX_ENTRY 		100
#define MAX_TRANSFORMS 	10000

static struct {
	batch_registry_entry_t *entry;  // list of batch entries
	uint32_t count;                 // count of entries in the list
} batch_registry;

static memory_t arena;



/* The function insert instance date to (transform adn uv_rect)
 * to the batch registry entry at positio i */
static void add_instance(mat4 transform, vec4 uv_rect,  uint32_t i) {
	if (batch_registry.entry[i].count >= MAX_TRANSFORMS-1) {
		/* TODO:
		 * need to decide what to do in this case. Maybe save the current information
		 * flush out the BR forcing the renderer to draw and now with BR cleaned up
		 * add back the saved transform. For now just return */
		fprintf(stderr, "Could not add instance data to the Batch Regitry\n");
		return;
	}
	batch_registry.entry[i].transforms[batch_registry.entry[i].count] = transform;
	batch_registry.entry[i].uv_rect[batch_registry.entry[i].count] = uv_rect;
	batch_registry.entry[i].count ++;
}

/* Create a new entry in the BR and preallocate the space for MAX_TRANSFORM
 * transforms */
static void add_new_entry(mesh_t* mesh, material_t* material, mat4 transform, mat4 vp, vec4 uv_rect) {
	if (batch_registry.count >= MAX_ENTRY-1) {
		/* TODO:
		 * need to decide what to do in this case. Maybe save the current information
		 * flush out the BR forcing the renderer to draw and now with BR cleaned up
		 * add back the saved transform. For now just return */
		fprintf(stderr, "Could not add a new entry to the Batch Regitry\n");
		return;
	}
	batch_registry.entry[batch_registry.count].transforms = arena_alloc(&arena, sizeof(mat4)*MAX_TRANSFORMS);
	batch_registry.entry[batch_registry.count].uv_rect = arena_alloc(&arena, sizeof(vec4)*MAX_TRANSFORMS);
	batch_registry.entry[batch_registry.count].mesh = mesh;
	batch_registry.entry[batch_registry.count].material = material;
	batch_registry.entry[batch_registry.count].vp = vp;
	batch_registry.entry[batch_registry.count].count = 0;
	add_instance(transform, uv_rect, batch_registry.count);
	batch_registry.count ++;
}

void batch_init(void) {
	arena_create(&arena,
			sizeof(batch_registry_entry_t)*MAX_ENTRY +
			sizeof(mat4)*MAX_TRANSFORMS*MAX_ENTRY +
            sizeof(vec4)*MAX_TRANSFORMS*MAX_ENTRY);
	batch_registry.entry = arena_alloc(&arena, sizeof(batch_registry_entry_t)*MAX_ENTRY);
	batch_registry.count = 0;
}

void batch_cleanup(void) {
	arena_reset(&arena);
	batch_registry.entry = arena_alloc(&arena, sizeof(batch_registry_entry_t)*MAX_ENTRY);
	batch_registry.count = 0;
}

void batch_shutdown(void) {
	arena_destroy(&arena);
}

void batch_flush(void) {
	;
}

/* The function travers the entries in the BR and if find an entry
 * the match same mesh and material then add the transform to the list
 * of trasforms for that entry. If no matching (material and mesh) is find
 * then a new entry in the BR is created for this new combination */
void batch_push(mesh_t* mesh, material_t* material, mat4 transform, vec4 uv_rect, mat4 pv) {
	if (batch_registry.count == 0) {
		add_new_entry(mesh, material, transform, pv, uv_rect);
		return;
	}

	for (uint32_t i = 0; i < batch_registry.count; i++) {
		if (material == batch_registry.entry[i].material
				&& mesh == batch_registry.entry[i].mesh) {
			add_instance(transform, uv_rect, i);
			return;
		}
	}
	add_new_entry(mesh, material, transform, pv, uv_rect);
}

uint32_t  batch_size(void) {
	return batch_registry.count;
}


batch_registry_entry_t batch_get_entry(uint32_t index) {
	return batch_registry.entry[index];
}
