// frontend.c

#define STB_MATH3D_IMPLEMENTATION
#include "stb_math3d.h"
#define STB_MEMORY_IMPLEMENTATION
#include "stb_memory.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#include "cubit.h"
#include "backend.h"
#include "batch.h"
#include "material.h"
#include "shader.h"
#include "object.h"
#include "mesh.h"
#include "shadow.h"
#include "camera.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


static viewport_resized_func vieport_resized;
static float shadow_distance = 50.0f;
static camera_t* active_camera; // The camera the player is using (focus active)


void shadow_distance_set(float sd) { shadow_distance = sd; }
float shadow_distance_get(void)    { return shadow_distance; }


void register_viewport_resized_callback(viewport_resized_func callback) {
	vieport_resized = callback;
}

void viewport_resized(int32_t width, int32_t height) {
	if (vieport_resized) vieport_resized(width, height);
}


float lerp_precise(float a, float b, float t) { return (1.0f - t) * a + t * b; }
float lerp(float a, float b, float t)         { return a + (b - a) * t; }


void fill_screen(color_t color) {
	render_command_t cmd;
	cmd.type = R_CMD_CLEAR;
	cmd.clear.color = color;
	cmd.clear.flags = COLOR_BUFFER;
	renderer_submit(&cmd);
}


void mesh_draw(object3d_t* obj, mat4 pv) {
	render_command_t cmd;
	cmd.type = R_CMD_DRAW;
	cmd.draw.transform = mat4_multiply(pv, object3d_get_transform(obj));
	mesh_t* mesh = object3d_get_mesh(obj);
	cmd.draw.vao = mesh->vao;
	cmd.draw.vertex_count = mesh->vertex_count;
	renderer_submit(&cmd);
}


void application_quit(void) {
	backend_shutdown();
}


void mesh_draw_instanced(mesh_t* mesh, mat4* transforms, uint32_t count, mat4 vp) {
	render_command_t cmd;
	cmd.type = R_CMD_DRAW_INSTANCED;
	cmd.instanced_draw.vp = vp;
	cmd.instanced_draw.m = transforms;
	cmd.instanced_draw.vao = mesh->vao;
	cmd.instanced_draw.vbo = mesh->instance_vbo_transform;
	cmd.instanced_draw.vertex_count = mesh->vertex_count;
	cmd.instanced_draw.instance_count = count;
	renderer_submit(&cmd);
}


void ambient_factor_set(float f) { backend_ambient_factor_set(f); }
float ambient_factor_get(void)   { return backend_ambient_factor_get(); }


// Once-per-frame call from the game. After Stage 23, this is the only point
// of contact between the game and the 3D rendering pipeline (apart from
// object3d_* setters): the game says "this is the camera", the engine does
// the rest in renderer_draw via object3d_collect_into_batch.
void camera_set_active(camera_t* c) {
	active_camera = c;
	camera_update(c);
	vec3 p = camera_get_position(c);
	mat4 vp = camera_get_view_projection_matrix(c);

	backend_set_camera_position(p);
	backend_set_view_matrix(camera_get_view_matrix(c));

	// Hand the camera state to the batch so push_object can compute distances
	// and stamp the VP onto each group.
	batch_set_camera(vp, p);

	// Cascade split distances and per-cascade frustum corners for CSM.
	shadow_compute_split_distances(c->near, shadow_distance);
	shadow_atlas_t* atlas = shadow_atlas_get();
	for (uint32_t i = 0; i < atlas->cascade_count; i++) {
		vec3 corners[8];
		camera_get_frustum_corners(c, corners,
			atlas->split_distances[i],
			atlas->split_distances[i + 1]);
		backend_set_cascade_corners(i, corners);
	}

	// Full frustum corners (used by spot lights — they ignore them but
	// shadow_map_update still receives a pointer)
	vec3 corners[8];
	camera_get_frustum_corners(c, corners, c->near, shadow_distance);
	backend_set_active_frustum_corners(corners);
}


camera_t* camera_get_active(void) {
	return active_camera;
}
