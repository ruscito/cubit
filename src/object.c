// object.c

#include "object.h"
#include "stb_math3d.h"
#include "mesh.h"
#include "material.h"
#include "texture.h"
#include "batch.h"
#include "camera.h"

#include <stdio.h>
#include <stdlib.h>
#include <float.h>


// Forward decl — defined in frontend.c. We don't include cubit.h here
// because that's the game-facing header; this is engine code.
extern camera_t* camera_get_active(void);


static struct {
	object3d_t objects[MAX_OBJECTS3D];
	uint32_t count;        // number of active objects (debug only)
} object_table;


// Resets a slot to default-but-inactive state. Called on init and on destroy
// so a freed slot is always in a known-clean state when reallocated.
static void reset_object(object3d_t* obj) {
	obj->position    = (vec3){0.0f, 0.0f, 0.0f};
	obj->rotation    = (vec3){0.0f, 0.0f, 0.0f};
	obj->scale       = (vec3){1.0f, 1.0f, 1.0f};
	obj->color       = COLOR_WHITE;
	obj->uv_rect     = (vec4){0.0f, 0.0f, 1.0f, 1.0f};
	obj->mesh        = NULL;
	obj->material    = NULL;
	obj->active      = false;
	obj->transform   = mat4_identity();
	obj->aabb        = (aabb_t){0};
}


// Rebuilds transform = T(position) × R_z × R_y × R_x × S(scale).
// Order chosen to match the rotate_* helpers' incremental behavior.
static void update_transform(object3d_t* obj) {
	mat4 s  = mat4_scale(obj->scale.x, obj->scale.y, obj->scale.z);
	mat4 rx = mat4_rotate_x(obj->rotation.x);
	mat4 ry = mat4_rotate_y(obj->rotation.y);
	mat4 rz = mat4_rotate_z(obj->rotation.z);
	mat4 t  = mat4_translate(obj->position.x, obj->position.y, obj->position.z);

	mat4 r  = mat4_multiply(mat4_multiply(rz, ry), rx);
	mat4 rs = mat4_multiply(r, s);
	obj->transform = mat4_multiply(t, rs);
}


// Recomputes the world-space AABB from the mesh's local AABB and the current
// transform. No-op when the object has no mesh yet — common during the
// "new + set_mesh + set_position" sequence: the AABB lands on the second
// call that has all the inputs available.
static void update_aabb(object3d_t* obj) {
	if (!obj->mesh) return;

	aabb_t local = obj->mesh->aabb;
	float* m = obj->transform.m;

	float cx[2] = { local.min_x, local.max_x };
	float cy[2] = { local.min_y, local.max_y };
	float cz[2] = { local.min_z, local.max_z };

	obj->aabb.min_x =  FLT_MAX;
	obj->aabb.max_x = -FLT_MAX;
	obj->aabb.min_y =  FLT_MAX;
	obj->aabb.max_y = -FLT_MAX;
	obj->aabb.min_z =  FLT_MAX;
	obj->aabb.max_z = -FLT_MAX;

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			for (int k = 0; k < 2; k++) {
				float wx = m[0]*cx[i] + m[4]*cy[j] + m[8] *cz[k] + m[12];
				float wy = m[1]*cx[i] + m[5]*cy[j] + m[9] *cz[k] + m[13];
				float wz = m[2]*cx[i] + m[6]*cy[j] + m[10]*cz[k] + m[14];

				if (wx < obj->aabb.min_x) obj->aabb.min_x = wx;
				if (wx > obj->aabb.max_x) obj->aabb.max_x = wx;
				if (wy < obj->aabb.min_y) obj->aabb.min_y = wy;
				if (wy > obj->aabb.max_y) obj->aabb.max_y = wy;
				if (wz < obj->aabb.min_z) obj->aabb.min_z = wz;
				if (wz > obj->aabb.max_z) obj->aabb.max_z = wz;
			}
		}
	}
}


// Convenience: every setter ends with this. We pay the matrix multiply and
// the 8-corner AABB transform on every setter call. At MAX_OBJECTS3D = 10000
// this is invisible on profilers as long as setters are not called in tight
// inner loops; if a future workload hits that case, we can re-introduce a
// dirty flag without changing the public API.
static void recompute(object3d_t* obj) {
	update_transform(obj);
	update_aabb(obj);
}


// ---- Lifecycle ----

void object3d_init(void) {
	object_table.count = 0;
	for (uint32_t i = 0; i < MAX_OBJECTS3D; i++) {
		reset_object(&object_table.objects[i]);
	}
}


void object3d_shutdown(void) {
	for (uint32_t i = 0; i < MAX_OBJECTS3D; i++) {
		reset_object(&object_table.objects[i]);
	}
	object_table.count = 0;
}


object3d_t* object3d_new(void) {
	for (uint32_t i = 0; i < MAX_OBJECTS3D; i++) {
		if (!object_table.objects[i].active) {
			object3d_t* obj = &object_table.objects[i];
			reset_object(obj);
			obj->material = material_get_default();
			obj->active   = true;
			update_transform(obj); // identity, but keep cached state consistent
			object_table.count++;
			return obj;
		}
	}
	fprintf(stderr, "object3d_new: object table full (MAX_OBJECTS3D = %d)\n",
	        MAX_OBJECTS3D);
	return NULL;
}


void object3d_destroy(object3d_t* obj) {
	if (!obj || !obj->active) return;
	reset_object(obj);
	object_table.count--;
}


// ---- Setters ----

void object3d_set_position(object3d_t* obj, vec3 pos) {
	obj->position = pos;
	recompute(obj);
}

void object3d_set_rotation(object3d_t* obj, vec3 euler_radians) {
	obj->rotation = euler_radians;
	recompute(obj);
}

void object3d_set_scale(object3d_t* obj, vec3 s) {
	obj->scale = s;
	recompute(obj);
}

void object3d_set_color(object3d_t* obj, color_t c) {
	obj->color = c;
	// Color does not affect transform or AABB — no recompute needed.
}

void object3d_set_mesh(object3d_t* obj, mesh_t* mesh) {
	obj->mesh = mesh;
	recompute(obj); // AABB now becomes meaningful
}

void object3d_set_material(object3d_t* obj, material_t* material) {
	obj->material = material;
	// Material does not affect transform or AABB.
}

void object3d_set_uv_rect(object3d_t* obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	// Half-texel inset to prevent atlas bleeding. Identical formula to
	// object2d_set_uv_rect — they share the asset pipeline contract.
	float rect_x = (float)x + 0.5f;
	float rect_y = (float)y + 0.5f;
	float rect_z = (float)(x + w) - 0.5f;
	float rect_w = (float)(y + h) - 0.5f;

	texture_t* tex = obj->material->diffuse_texture;
	obj->uv_rect.x = rect_x / (float)tex->width;
	obj->uv_rect.y = rect_y / (float)tex->height;
	obj->uv_rect.z = rect_z / (float)tex->width;
	obj->uv_rect.w = rect_w / (float)tex->height;
}

void object3d_set_active(object3d_t* obj, bool active) {
	// Disabling an object keeps the slot allocated; collect_into_batch will
	// just skip it. Use object3d_destroy() to truly release the slot.
	obj->active = active;
}


void object3d_move(object3d_t* obj, vec3 delta) {
	obj->position.x += delta.x;
	obj->position.y += delta.y;
	obj->position.z += delta.z;
	recompute(obj);
}

void object3d_rotate_x(object3d_t* obj, float angle_radians) {
	obj->rotation.x += angle_radians;
	recompute(obj);
}

void object3d_rotate_y(object3d_t* obj, float angle_radians) {
	obj->rotation.y += angle_radians;
	recompute(obj);
}

void object3d_rotate_z(object3d_t* obj, float angle_radians) {
	obj->rotation.z += angle_radians;
	recompute(obj);
}


// ---- Getters ----

vec3 object3d_get_position(object3d_t* obj) { return obj->position; }
vec3 object3d_get_rotation(object3d_t* obj) { return obj->rotation; }
vec3 object3d_get_scale(object3d_t* obj)    { return obj->scale; }
color_t object3d_get_color(object3d_t* obj) { return obj->color; }
vec4 object3d_get_uv_rect(object3d_t* obj)  { return obj->uv_rect; }
mesh_t* object3d_get_mesh(object3d_t* obj)  { return obj->mesh; }
material_t* object3d_get_material(object3d_t* obj) { return obj->material; }
mat4 object3d_get_transform(object3d_t* obj){ return obj->transform; }
aabb_t object3d_get_aabb(object3d_t* obj)   { return obj->aabb; }
bool object3d_get_active(object3d_t* obj)   { return obj->active; }


// ---- Iteration & collect ----

object3d_t* object3d_iter(object3d_t* prev) {
	uint32_t start = 0;
	if (prev) start = (uint32_t)(prev - object_table.objects) + 1;
	for (uint32_t i = start; i < MAX_OBJECTS3D; i++) {
		if (object_table.objects[i].active) return &object_table.objects[i];
	}
	return NULL;
}


// Per-frame collect: walks the pool, frustum-tests each active object against
// the active camera, and pushes the survivors into the 3D batch. The batch
// itself decides whether each entry goes to the opaque or transparent
// registry based on material->opacity.
//
// Camera comes from camera_get_active() — same convention as the old
// submit_object3d() path. The game just calls camera_set_active() once per
// frame and the rest is implicit.
void object3d_collect_into_batch(void) {
	camera_t* cam = camera_get_active();
	if (!cam) return; // no camera bound this frame, nothing to render

	for (uint32_t i = 0; i < MAX_OBJECTS3D; i++) {
		object3d_t* obj = &object_table.objects[i];
		if (!obj->active) continue;
		if (!obj->mesh)   continue;     // object with no mesh: skip silently
		if (!obj->material) continue;
		if (!camera_is_object3d_visible(cam, obj)) continue;

		batch_push_object(obj);
	}
}
