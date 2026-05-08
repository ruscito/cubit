// object.h
//
// 3D retained-object API. Stage 23 brings this module in line with the 2D
// pool pattern established in Stage 22:
//   - Fixed-size pool with stable pointers (active flag, no malloc per object).
//   - Explicit position / rotation (vec3 of euler angles, radians) / scale.
//   - Per-instance color tinting, baked CPU-side into the batch.
//   - No dirty flag — every setter recomputes the model matrix and AABB
//     immediately, and the cost is negligible at our object counts.
//   - object3d_collect_into_batch() walks active objects and feeds them into
//     the 3D batch every frame. The game never calls submit_object3d (it has
//     been removed from the public API).

#ifndef OBJECT_H_
#define OBJECT_H_

#include "cubit_types.h"


#define MAX_OBJECTS3D 10000


struct object3d_t {
	// Transform state (game-facing, edited via setters)
	vec3 position;
	vec3 rotation;          // euler angles in radians: (pitch_x, yaw_y, roll_z)
	vec3 scale;             // per-axis scale, default (1,1,1)

	// Visuals
	color_t color;          // per-instance tint, multiplied with material->surface_color
	vec4 uv_rect;           // atlas sub-region in normalized UV coords
	mesh_t* mesh;
	material_t* material;

	// Lifetime
	bool active;            // false = slot free, or disabled object

	// Cached, recomputed by every setter
	mat4 transform;         // model matrix in world space
	aabb_t aabb;            // world-space AABB derived from mesh + transform
};


// ---- Lifecycle ----

void object3d_init(void);
void object3d_shutdown(void);

object3d_t* object3d_new(void);
void object3d_destroy(object3d_t* obj);


// ---- Setters (each recomputes transform + AABB if a mesh is bound) ----

void object3d_set_position(object3d_t* obj, vec3 pos);
void object3d_set_rotation(object3d_t* obj, vec3 euler_radians);
void object3d_set_scale(object3d_t* obj, vec3 s);
void object3d_set_color(object3d_t* obj, color_t c);
void object3d_set_mesh(object3d_t* obj, mesh_t* mesh);
void object3d_set_material(object3d_t* obj, material_t* material);
void object3d_set_uv_rect(object3d_t* obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void object3d_set_active(object3d_t* obj, bool active);

void object3d_move(object3d_t* obj, vec3 delta);
void object3d_rotate_x(object3d_t* obj, float angle_radians);
void object3d_rotate_y(object3d_t* obj, float angle_radians);
void object3d_rotate_z(object3d_t* obj, float angle_radians);


// ---- Getters ----

vec3 object3d_get_position(object3d_t* obj);
vec3 object3d_get_rotation(object3d_t* obj);
vec3 object3d_get_scale(object3d_t* obj);
color_t object3d_get_color(object3d_t* obj);
vec4 object3d_get_uv_rect(object3d_t* obj);
mesh_t* object3d_get_mesh(object3d_t* obj);
material_t* object3d_get_material(object3d_t* obj);
mat4 object3d_get_transform(object3d_t* obj);
aabb_t object3d_get_aabb(object3d_t* obj);
bool object3d_get_active(object3d_t* obj);


// ---- Iteration & engine-internal collect ----

// Returns the next active object after 'prev', or NULL when iteration ends.
// Pass NULL as 'prev' to start.
object3d_t* object3d_iter(object3d_t* prev);

// Pushes every active object into the 3D batch using the active camera.
// Called once per frame by the backend, mirroring object2d_collect_into_batch.
// The game never calls this directly.
void object3d_collect_into_batch(void);


#endif // OBJECT_H_
