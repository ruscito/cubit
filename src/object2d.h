// object2d.h

#ifndef OBJECT2D_H_
#define OBJECT2D_H_

#include "cubit_types.h"


#define MAX_OBJECTS2D 10000


struct object2d_t {
	vec2 position;          // center of the sprite in virtual units
	vec2 size;              // width and height in virtual units
	float rotation;         // radians, around the sprite center

	color_t color;          // per-instance tint
	vec4 uv_rect;           // atlas sub-region in normalized UV coords
	bool flip_x;            // horizontal flip (applied via uv_rect swap at submit time)
	bool flip_y;            // vertical flip

	material2d_t* material; // defaults to material2d_get_default()
	camera2d_t* camera;     // defaults to camera2d_get_ui()
	int32_t layer;          // z-order within camera (higher = drawn on top)

	bool active;            // false = slot free, or disabled object

	// cached
	mat4 model_matrix;
	bool dirty;
};


void object2d_init(void);
void object2d_shutdown(void);

object2d_t* object2d_new(void);
void object2d_destroy(object2d_t* obj);

void object2d_set_position(object2d_t* obj, vec2 position);
void object2d_set_size(object2d_t* obj, vec2 size);
void object2d_set_rotation(object2d_t* obj, float radians);
void object2d_set_color(object2d_t* obj, color_t c);
void object2d_set_uv_rect(object2d_t* obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void object2d_set_flip(object2d_t* obj, bool flip_x, bool flip_y);
void object2d_set_material(object2d_t* obj, material2d_t* material);
void object2d_set_camera(object2d_t* obj, camera2d_t* camera);
void object2d_set_layer(object2d_t* obj, int32_t layer);
void object2d_set_active(object2d_t* obj, bool active);

void object2d_move(object2d_t* obj, vec2 delta);

vec2 object2d_get_position(object2d_t* obj);
vec2 object2d_get_size(object2d_t* obj);
float object2d_get_rotation(object2d_t* obj);
color_t object2d_get_color(object2d_t* obj);
vec4 object2d_get_uv_rect(object2d_t* obj);
material2d_t* object2d_get_material(object2d_t* obj);
camera2d_t* object2d_get_camera(object2d_t* obj);
int32_t object2d_get_layer(object2d_t* obj);
bool object2d_get_active(object2d_t* obj);

// Returns the cached model matrix, recomputing if dirty.
// Called by the 2D batch system.
mat4 object2d_get_model_matrix(object2d_t* obj);

// Returns the uv_rect with flip_x/flip_y applied (coords swapped).
// Called by the 2D batch system — the object itself keeps logical values intact.
vec4 object2d_get_effective_uv_rect(object2d_t* obj);

// Iteration for the batch renderer. Returns the next active object after 'prev',
// or NULL when iteration ends. Pass NULL as 'prev' to start.
object2d_t* object2d_iter(object2d_t* prev);


// Pushes every active object into the 2D batch in pool order.
// Called once per frame by the frontend, between batch2d_begin_frame()
// and batch2d_end_frame(). The pool is the source of truth for retained
// sprites — this is the bridge that feeds them into the rendering pipeline.
//
// Implemented as a single function (rather than exposing a callback) to keep
// the pool's internal structure private to this module: future changes from
// linear scan to free-list or generation counters will not touch the caller.
void object2d_collect_into_batch(void);


#endif
