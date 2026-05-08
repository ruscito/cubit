// batch.h
//
// 3D batch registry. Stage 23 brings the 3D batch architecture in line with
// the 2D one (Stage 22): sort-key-driven grouping, AoS records during collect,
// SoA upload at draw time, and a single API entry point — batch_push_object()
// — fed by object3d_collect_into_batch() rather than per-object game-side
// submission.
//
// What changed compared to the pre-Stage-23 batch:
//   - The opaque registry no longer groups by linear scan over (mesh, material).
//     Records are sorted by a packed uint64 sort_key, then grouped by walking
//     the sorted array in a single pass. Same deterministic grouping, scales
//     naturally as object counts grow.
//   - The transparent registry shares the same record/sort-key infrastructure;
//     pass=transparent dominates the high bits of the key, depth dominates the
//     low bits — back-to-front order falls out of the unsigned sort.
//   - Per-instance color is uploaded alongside transform and uv_rect. Color is
//     baked CPU-side as object.color × material.surface_color, so the lit and
//     unlit shaders need only one instance attribute and no per-batch color
//     uniform.
//
// Game code never touches this module. The frontend opens/closes the frame,
// object3d_collect_into_batch() pushes entries, the backend consumes groups.

#ifndef BATCH_H_
#define BATCH_H_

#include "cubit_types.h"


// One batch group corresponds to one glDrawElementsInstanced call.
// All instances in [instance_offset, instance_offset + instance_count) share
// the same mesh, material and shader.
typedef struct {
	mesh_t*     mesh;
	material_t* material;
	mat4        vp;             // view-projection of the active camera
	uint32_t    instance_offset; // index into the contiguous instance arrays
	uint32_t    instance_count;
} batch_group_t;


// Per-instance GPU data — uploaded to the mesh's instance VBOs each frame.
// Kept as three separate arrays in the upload phase for layout symmetry with
// the 2D pipeline.
typedef struct {
	mat4    transform;
	vec4    uv_rect;
	color_t color;
} batch_instance_t;


// ---- Lifecycle ----

void batch_init(void);
void batch_shutdown(void);


// ---- Frame production ----

// Resets the per-frame arena and counters. Called once at the start of every
// frame, before any push.
void batch_begin_frame(void);

// Appends one 3D object into the batch. Routes to opaque or transparent
// based on material->opacity. The active camera's VP is read once per frame
// in batch_set_camera() — the caller does not have to pass it in.
void batch_push_object(object3d_t* obj);

// Sorts records, builds the group arrays, uploads CPU-side baked color into
// each group's color array. Called at end of frame production phase.
void batch_end_frame(void);


// ---- Frame consumption ----

// Opaque groups, sorted by (material, mesh) for shader/material switch
// minimization. Front-to-back ordering is left to the depth buffer.
const batch_group_t* batch_get_opaque_groups(void);
uint32_t batch_get_opaque_group_count(void);

// Transparent groups, sorted back-to-front by depth. Each transparent group
// is typically count=1 (no instancing across transparent objects), but the
// API stays uniform.
const batch_group_t* batch_get_transparent_groups(void);
uint32_t batch_get_transparent_group_count(void);

// Pointer to the contiguous per-instance arrays for the given group's
// pass. The backend dispatches by pass internally; these are the source
// arrays it copies into the mesh's instance VBOs before each draw.
const batch_instance_t* batch_get_opaque_instances(void);
const batch_instance_t* batch_get_transparent_instances(void);


// ---- Frame-wide camera state ----

// Caches the active camera's VP and position once per frame. Must be called
// after batch_begin_frame and before any batch_push_object call.
void batch_set_camera(mat4 vp, vec3 camera_position);


#endif // BATCH_H_
