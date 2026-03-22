// mesh.c

#include "mesh.h"
#include "backend.h"

#include <stdlib.h>
#include <stdio.h>
#include <float.h>

mesh_t* mesh_create(void *pos, void* nor, void *uv, void *tg, uint32_t *indices, uint32_t vertex_count, uint32_t index_count) {
	mesh_t* m = malloc(sizeof(*m));
	if (m == NULL) {
		fprintf(stderr, "Failed to allocate memory for a mesh object!\n");
		return NULL;
	}

	float *posf = (float*) pos;
	m->aabb.min_x = FLT_MAX;
	m->aabb.max_x = -FLT_MAX;
	m->aabb.min_y = FLT_MAX;
	m->aabb.max_y = -FLT_MAX;
	m->aabb.min_z = FLT_MAX;
	m->aabb.max_z = -FLT_MAX;
	for (uint32_t i = 0; i < vertex_count; i++) {
			if (posf[i*3+0] >= m->aabb.max_x) m->aabb.max_x = posf[i*3+0];
			if (posf[i*3+0] <= m->aabb.min_x) m->aabb.min_x = posf[i*3+0];
			if (posf[i*3+1] >= m->aabb.max_y) m->aabb.max_y = posf[i*3+1];
			if (posf[i*3+1] <= m->aabb.min_y) m->aabb.min_y = posf[i*3+1];
			if (posf[i*3+2] >= m->aabb.max_z) m->aabb.max_z = posf[i*3+2];
			if (posf[i*3+2] <= m->aabb.min_z) m->aabb.min_z = posf[i*3+2];
	}

	backend_mesh_new(m, pos, nor, uv, tg, indices, vertex_count, index_count);

	return m;
}

void mesh_destroy(mesh_t *m) {
	backend_mesh_destroy(m);
	free(m);
}


