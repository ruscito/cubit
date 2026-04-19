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

#define DEFAULT_INDEX 0

static viewport_resized_func vieport_resized;
static float shadow_distance = 50.0f;
static camera_t* active_camera; // The camera the player is using (focus active)
static mat4 active_mvp; // the active camera vpm

void shadow_distance_set(float sd) {
    shadow_distance = sd;
}

float shadow_distance_get(void) {
    return shadow_distance;
}


/* API:
 * Called from the game client to register a callback function
 * to catch window resizing */
void register_viewport_resized_callback(viewport_resized_func callback) {
	vieport_resized = callback;
}

/* Called by the backend when the framebuffer changes, this
 * function can callback the game client */
void viewport_resized(int32_t width, int32_t height) {
	if (vieport_resized) vieport_resized(width, height);
}


float lerp_precise(float a, float b, float t) {
    return (1.0f - t) * a + t * b;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

void fill_screen(color_t color) {
    render_command_t cmd;
    cmd.type = R_CMD_CLEAR;
    cmd.clear.color = color;
    cmd.clear.flags = COLOR_BUFFER; // Defaults
    renderer_submit(&cmd);
}


void mesh_draw(object3d_t *obj, mat4 pv){
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

/* API:
 * Called from the game client to submit object to the renderer.
 * If the object get pushed without material then the engine
 * provide the default one */
void submit_object3d(object3d_t* obj) {
	if (camera_is_object3d_visible(active_camera, obj)) {
		batch_push(
			obj->mesh,
			obj->material,
			obj->transform,
            obj->uv_rect,
			active_mvp // camera_get_view_projection_matrix(camera)
		);

	}
}


void ambient_factor_set(float f) {
	backend_ambient_factor_set(f);
}

float ambient_factor_get(void) {
	return backend_ambient_factor_get();
}

void camera_set_active(camera_t* c) {
    active_camera = c;
    camera_update(c);
    vec3 p = camera_get_position(c);
    backend_set_camera_position(p);
    backend_set_view_matrix(camera_get_view_matrix(c));
    batch_set_camera_position(p);

    // Compute cascade split distances for the current frame
    shadow_compute_split_distances(c->near, shadow_distance);

    // Compute per-cascade frustum corners
    shadow_atlas_t* atlas = shadow_atlas_get();
    for (uint32_t i = 0; i < atlas->cascade_count; i++) {
        vec3 corners[8];
        camera_get_frustum_corners(c, corners,
            atlas->split_distances[i],
            atlas->split_distances[i + 1]);
        backend_set_cascade_corners(i, corners);
    }

    // Full fru 2stum corners (used by spot lights — they ignore them
    // but shadow_map_update still receives a pointer)
    vec3 corners[8];
    camera_get_frustum_corners(c, corners, c->near, shadow_distance);
    backend_set_active_frustum_corners(corners);

    active_mvp = camera_get_view_projection_matrix(c);
}

camera_t* camera_get_active(void) {
    return active_camera;
}

