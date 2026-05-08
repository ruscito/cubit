// batch2d.h
//
// 2D batch registry: collects all sprites submitted during a frame, sorts them
// by (camera priority -> layer -> blend mode -> texture -> submission order)
// and exposes the result to the backend overlay pass as two flat arrays:
// one of "groups" (each group = one draw call) and one of "instances"
// (the per-sprite data uploaded to the GPU once per frame).
//
// The game code never touches this module directly. It is driven by the
// frontend (which collects active object2d_t and immediate sprite_submit()
// calls into the batch) and consumed by the backend (which iterates the
// produced groups and emits glDrawElementsInstanced calls).

#ifndef BATCH2D_H_
#define BATCH2D_H_

#include "cubit_types.h"


// One group corresponds to exactly one glDrawElementsInstanced call.
// All instances in the [instance_offset, instance_offset + instance_count)
// range share the same camera, blend mode and texture.
typedef struct {
	camera2d_t*   camera;          // for view/projection uniforms
	blend_mode_t  blend_mode;      // for glBlendFunc selection
	uint32_t      texture_id;      // GL texture name to bind on slot 0
	uint32_t      instance_offset; // first instance index in the global instance array
	uint32_t      instance_count;  // number of instances in this group
} batch2d_group_t;


// Per-sprite GPU-side data, packed contiguously in the instance array.
// Layout matches the instanced vertex attributes of the 2D shader:
//   - model matrix (locations 6..9)
//   - uv_rect      (location 10)
//   - color        (location 11)
typedef struct {
	mat4    model;
	vec4    uv_rect;
	color_t color;
} batch2d_instance_t;


// ---- Lifecycle (called once at engine startup / shutdown) ----

// Allocates the arena, creates the shared sprite quad VAO and the instanced
// VBOs (model, uv_rect, color) sized for MAX_OBJECTS2D instances.
void batch2d_init(void);

// Frees the arena and destroys the quad VAO/VBOs.
void batch2d_shutdown(void);


// ---- Frame production API (called by the frontend) ----

// Resets the arena and the submission counter.
// Must be called once at the very start of each frame, before any push.
void batch2d_begin_frame(void);

// Appends one sprite to the registry. The submission_index is assigned
// internally as a monotonic counter, used as the final tiebreaker when sorting
// so that two sprites with otherwise identical sort keys preserve the order
// in which they were pushed.
//
// Pulls model_matrix and effective uv_rect from the object — meaning the
// caller does not need to flatten the object2d_t fields before calling.
void batch2d_push_object(object2d_t* obj);


// Appends one immediate sprite to the registry. Called by sprite_submit()
// (and, after step 9, by the refactored text system) to inject sprites that
// have no backing object2d_t.
//
// This is engine-internal API: the game never calls it directly. Use
// sprite_submit() from cubit.h instead.
//
// 'transform' is a full model matrix (the caller is responsible for whatever
// translate/rotate/scale combination it needs). 'color' is the per-instance
// tint and gets multiplied with material->color CPU-side, same as for
// retained objects — keeps the shader path uniform.
void batch2d_push_immediate(camera2d_t* camera,
                            int32_t layer,
                            material2d_t* material,
                            mat4 transform,
                            vec4 uv_rect,
                            color_t color);


// Sorts all submitted sprites, builds the group array and uploads the entire
// instance array to the GPU in a single glBufferSubData call.
// Must be called once at the end of the frame production phase, before the
// backend overlay pass starts consuming the groups.
void batch2d_end_frame(void);


// ---- Frame consumption API (called by the backend) ----

// Returns a pointer to the contiguous group array. Valid only between
// batch2d_end_frame() and the next batch2d_begin_frame().
const batch2d_group_t* batch2d_get_groups(void);

// Number of groups produced this frame (== number of draw calls to emit).
uint32_t batch2d_get_group_count(void);


// Binds the shared sprite quad VAO. The backend calls this once at the start
// of the overlay pass, then iterates groups and emits draws — no rebinding
// per group is needed because all groups share the same geometry.
void batch2d_bind_quad_vao(void);


// Instance VBO accessors. Used by the backend overlay pass to re-point
// per-instance attribute pointers at each group's slice of the contiguous
// buffer (the GL 3.3 equivalent of base-instance draws). Returning the raw
// GL handle is intentional: the backend does the pointer arithmetic, the
// batch module owns the buffers.
uint32_t batch2d_get_instance_vbo_model(void);
uint32_t batch2d_get_instance_vbo_uv_rect(void);
uint32_t batch2d_get_instance_vbo_color(void);


#endif // BATCH2D_H_
