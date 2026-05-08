// object2d.c

#include "object2d.h"
#include "material2d.h"
#include "camera2d.h"
#include "texture.h"
#include "batch2d.h"

#include <stdio.h>
#include <math.h>


static struct {
	object2d_t objects[MAX_OBJECTS2D];
	uint32_t count;        // number of active objects (for debugging)
} object_table;


static void reset_object(object2d_t* obj) {
	obj->position = (vec2){0.0f, 0.0f};
	obj->size = (vec2){100.0f, 100.0f};
	obj->rotation = 0.0f;
	obj->color = COLOR_WHITE;
	obj->uv_rect = (vec4){0.0f, 0.0f, 1.0f, 1.0f};
	obj->flip_x = false;
	obj->flip_y = false;
	obj->material = NULL;
	obj->camera = NULL;
	obj->layer = 0;
	obj->active = false;
	obj->model_matrix = mat4_identity();
	obj->dirty = true;
}


static void update_model_matrix(object2d_t* obj) {
	// model = translate(position) × rotate(rotation) × scale(size)
	// read right-to-left: scale the unit quad to the desired size, rotate
	// around the origin (which is the quad's center since quad is -0.5..0.5),
	// then translate to the final world position.
	mat4 s = mat4_scale(obj->size.x, obj->size.y, 1.0f);
	mat4 r = mat4_rotate_z(obj->rotation);
	mat4 t = mat4_translate(obj->position.x, obj->position.y, 0.0f);

	mat4 rs = mat4_multiply(r, s);
	obj->model_matrix = mat4_multiply(t, rs);

	obj->dirty = false;
}


void object2d_init(void) {
	object_table.count = 0;
	for (uint32_t i = 0; i < MAX_OBJECTS2D; i++) {
		reset_object(&object_table.objects[i]);
	}
}


void object2d_shutdown(void) {
	for (uint32_t i = 0; i < MAX_OBJECTS2D; i++) {
		reset_object(&object_table.objects[i]);
	}
	object_table.count = 0;
}


object2d_t* object2d_new(void) {
	// Find first inactive slot
	for (uint32_t i = 0; i < MAX_OBJECTS2D; i++) {
		if (!object_table.objects[i].active) {
			object2d_t* obj = &object_table.objects[i];
			reset_object(obj);
			obj->material = material2d_get_default();
			obj->camera = camera2d_get_ui();
			obj->active = true;
			object_table.count++;
			return obj;
		}
	}
	fprintf(stderr, "object2d_new: object table full (MAX_OBJECTS2D = %d)\n", MAX_OBJECTS2D);
	return NULL;
}


void object2d_destroy(object2d_t* obj) {
	if (!obj || !obj->active) return;
	reset_object(obj);
	object_table.count--;
}


void object2d_set_position(object2d_t* obj, vec2 position) {
	obj->position = position;
	obj->dirty = true;
}

void object2d_set_size(object2d_t* obj, vec2 size) {
	obj->size = size;
	obj->dirty = true;
}

void object2d_set_rotation(object2d_t* obj, float radians) {
	obj->rotation = radians;
	obj->dirty = true;
}

void object2d_set_color(object2d_t* obj, color_t c) {
	obj->color = c;
}

void object2d_set_uv_rect(object2d_t* obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	// Half-texel inset to prevent filtering from sampling tile edges
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

void object2d_set_flip(object2d_t* obj, bool flip_x, bool flip_y) {
	obj->flip_x = flip_x;
	obj->flip_y = flip_y;
}

void object2d_set_material(object2d_t* obj, material2d_t* material) {
	obj->material = material;
}

void object2d_set_camera(object2d_t* obj, camera2d_t* camera) {
	obj->camera = camera;
}

void object2d_set_layer(object2d_t* obj, int32_t layer) {
	obj->layer = layer;
}

void object2d_set_active(object2d_t* obj, bool active) {
	// Note: setting active=false on an existing object just disables it.
	// It stays in the slot and can be reactivated. Use object2d_destroy
	// to truly release the slot.
	obj->active = active;
}


void object2d_move(object2d_t* obj, vec2 delta) {
	obj->position.x += delta.x;
	obj->position.y += delta.y;
	obj->dirty = true;
}


vec2 object2d_get_position(object2d_t* obj) {
	return obj->position;
}

vec2 object2d_get_size(object2d_t* obj) {
	return obj->size;
}

float object2d_get_rotation(object2d_t* obj) {
	return obj->rotation;
}

color_t object2d_get_color(object2d_t* obj) {
	return obj->color;
}

vec4 object2d_get_uv_rect(object2d_t* obj) {
	return obj->uv_rect;
}

material2d_t* object2d_get_material(object2d_t* obj) {
	return obj->material;
}

camera2d_t* object2d_get_camera(object2d_t* obj) {
	return obj->camera;
}

int32_t object2d_get_layer(object2d_t* obj) {
	return obj->layer;
}

bool object2d_get_active(object2d_t* obj) {
	return obj->active;
}


mat4 object2d_get_model_matrix(object2d_t* obj) {
	if (obj->dirty) update_model_matrix(obj);
	return obj->model_matrix;
}


vec4 object2d_get_effective_uv_rect(object2d_t* obj) {
	// Apply flip by swapping the rect coordinates.
	// Shader's formula uv_final = uv * (zw - xy) + xy produces reversed UVs
	// when zw < xy, which is exactly the flip effect we want.
	vec4 r = obj->uv_rect;
	if (obj->flip_x) {
		float tmp = r.x;
		r.x = r.z;
		r.z = tmp;
	}
	if (obj->flip_y) {
		float tmp = r.y;
		r.y = r.w;
		r.w = tmp;
	}
	return r;
}


object2d_t* object2d_iter(object2d_t* prev) {
	uint32_t start = 0;
	if (prev) {
		start = (uint32_t)(prev - object_table.objects) + 1;
	}
	for (uint32_t i = start; i < MAX_OBJECTS2D; i++) {
		if (object_table.objects[i].active) return &object_table.objects[i];
	}
	return NULL;
}


// Walks the pool linearly and pushes every active object into the 2D batch.
// Pool order = submission order — sprites earlier in the pool win the
// submission_index tiebreak when they share (camera, layer, blend, texture)
// with a later sprite.
//
// The linear scan is the right call for now: MAX_OBJECTS2D = 10000 with a
// trivial active-flag check per slot fits in a fraction of a millisecond
// even at full saturation. If profiling later shows the empty-slot scan
// dominating, we can switch to a free-list of active indices without
// touching this function's signature.
void object2d_collect_into_batch(void) {
	for (uint32_t i = 0; i < MAX_OBJECTS2D; i++) {
		object2d_t* obj = &object_table.objects[i];
		if (obj->active) {
			batch2d_push_object(obj);
		}
	}
}
