// batch.h

#ifndef BATCH_H_
#define BATCH_H_

#include "cubit_types.h"

typedef struct {
	mesh_t* mesh;
	material_t* material;
	mat4* transforms;
	uint32_t count;			// How many transforms are currently stored
	mat4 vp;
} batch_registry_entry_t;



void batch_init(void);
void batch_cleanup(void);
void batch_shutdown(void);
void batch_push(mesh_t* mesh, material_t* material, mat4 transform, mat4 vp);
uint32_t batch_size(void);
batch_registry_entry_t batch_get_entry(uint32_t index);


#endif // BATCH_H_
