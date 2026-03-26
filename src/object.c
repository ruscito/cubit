// object.c

#include "object.h"
#include "stb_math3d.h"
#include "mesh.h"
#include "material.h"
#include "texture.h"

#include <stdlib.h>
#include <stdio.h>
#include <float.h>



/* It creates a new object3d and set all the default value */
object3d_t* object3d_new(void) {
	object3d_t  *obj = malloc(sizeof(*obj));
	if (!obj) {
		fprintf(stderr, "Failed to allocate object3d\n");
		exit(-1);
	}
	obj->transform = mat4_identity();
	obj->mesh = NULL;
	obj->material = material_get_default();
    obj->uv_rect = (vec4){0, 0, 1, 1};
	return obj;
}

static void update_aabb(object3d_t *obj) {
	if (!obj->mesh) return;

	aabb_t local = obj->mesh->aabb;
	float *m = obj->transform.m;

	// Build the 8 corners of the local AABB
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
				// Transform local corner by model matrix
				float wx = m[0]*cx[i] + m[4]*cy[j] + m[8]*cz[k]  + m[12];
				float wy = m[1]*cx[i] + m[5]*cy[j] + m[9]*cz[k]  + m[13];
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

	obj->dirty = false;
}

void  object3d_set_mesh(object3d_t *obj, mesh_t* mesh) {
	obj->mesh = mesh;
	obj->dirty = true;
}

void object3d_set_material(object3d_t *obj, material_t* material) {
	obj->material = material;
}

vec3 object3d_get_position(object3d_t *obj) {
	return obj->position;
}

material_t* object3d_get_material(object3d_t *obj) {
	return obj->material;
}

void object3d_set_position(object3d_t *obj, vec3 pos) {
	obj->position = pos;
	// I could do that
	// mat4_multiply(obj->transform, mat4_translate(pos.x, pos.y, pos.z));
	// but is faster to do:
	obj->transform.m[12] = pos.x;
	obj->transform.m[13] = pos.y;
	obj->transform.m[14] = pos.z;
	obj->dirty = true;
}

mesh_t* object3d_get_mesh(object3d_t *obj) {
	return obj->mesh;
}

mat4 object3d_get_transform(object3d_t *obj) {
	return obj->transform;
}

void object3d_destroy(object3d_t *obj) {
	free(obj);
}

void object3d_rotate_x(object3d_t *obj, float angle) {
	obj->transform = mat4_multiply(obj->transform, mat4_rotate_x(angle * DEG2RAD));
	obj->dirty = true;
}


void object3d_rotate_y(object3d_t *obj, float angle) {
	obj->transform = mat4_multiply(obj->transform, mat4_rotate_y(angle * DEG2RAD));
	obj->dirty = true;
}

void object3d_rotate_z(object3d_t *obj, float angle) {
	obj->transform = mat4_multiply(obj->transform, mat4_rotate_z(angle * DEG2RAD));
	obj->dirty = true;
}

aabb_t object3d_get_aabb(object3d_t *obj) {
	if (obj->dirty) update_aabb(obj);
	return obj->aabb;
}

/* Normalize and assign the object uv_rect */
void object3d_set_uv_rect(object3d_t* obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    // normalization
    obj->uv_rect.x = (float)x / (float)obj->material->diffuse_texture->width;
    obj->uv_rect.y = (float)y / (float)obj->material->diffuse_texture->height;
    obj->uv_rect.z = (float)(x+w) / (float)obj->material->diffuse_texture->width;
    obj->uv_rect.w = (float)(y+h) / (float)obj->material->diffuse_texture->height;
}

