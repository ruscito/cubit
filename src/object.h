// object.h

#ifndef OBJECT_H_
#define OBJECT_H_

#include "cubit_types.h"

// Maybe I should add also the shader

struct object3d_t{
	vec3 position;
	mat4 transform;
	mesh_t* mesh;
	material_t* material;
	aabb_t aabb;
	bool dirty;
	float bounding;
};

#endif
