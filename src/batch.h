// batch.h

#ifndef BATCH_H_
#define BATCH_H_

#include "cubit_types.h"

typedef struct {
	mesh_t* mesh;
	material_t* material;
    vec4* uv_rect;
	mat4* transforms;       // Transform is the Model matrix
	uint32_t count;			// How many transforms are currently
                            // stored in the batch entry
	mat4 vp;                // View and Projection
} batch_registry_entry_t;



void batch_init(void);
void batch_cleanup(void);
void batch_shutdown(void);
void batch_push(mesh_t* mesh, material_t* material, mat4 transform, vec4  uv_rect, mat4 pv);
uint32_t batch_size(void);
batch_registry_entry_t batch_get_entry(uint32_t index);


#endif // BATCH_H_
