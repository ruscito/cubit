// mesh.h

#ifndef MESH_H_
#define MESH_H_

#include "cubit_types.h"


struct mesh_t {
	uint32_t vao;
	uint32_t vbo_position;
	uint32_t vbo_normal;
	uint32_t vbo_uv;
	uint32_t vbo_tangent;
	uint32_t instance_vbo_transform;
	uint32_t instance_vbo_uv_rect;
	uint32_t ebo;
	uint32_t vertex_count;
	uint32_t index_count;
	aabb_t aabb;
};




#endif
