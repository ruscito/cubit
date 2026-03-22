// shadow.h

#ifndef SHADOW_H_
#define SHADOW_H_

#include "cubit_types.h"

#define DEFAULT_SM_SIZE 8192
#define DEFAULT_SM_DISTANCE 20

struct shadow_map_t {
	uint32_t id;
	uint32_t fbo;
	uint32_t size; // framebuffer is square 1024x1024 by default
	mat4 vp;
	bool dirty;
};


void shadow_map_update(shadow_map_t* s, light_t* light, vec3* corners);

#endif
