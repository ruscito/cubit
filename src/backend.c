// backend.c

#include "backend.h"
#include "batch.h"
#include "object.h"
#include "material.h"
#include "material2d.h"
#include "shader.h"
#include "light.h"
#include "mesh.h"
#include "texture.h"
#include "shadow.h"
#include "text.h"
#include "batch2d.h"
#include "object2d.h"
#include "camera2d.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


#define MAX_QUEUE 1000000
#define DEFAULT_AMBIENT_FACTOR 0.1f
#define MAX_DT 0.05
#define FIXED_STEP (1.0/60.0)


struct gfx_context {
	int width;
	int height;
	int framebuffer_width;
	int framebuffer_height;
	int fps;
	const char* title;
};

static GLFWwindow *window;
static int32_t inside_previous_state = 0;
static int32_t inside_current_state  = 0;
static double last_frame_time = 0.0;
static vec3 camera_position;
static vec3 active_frustum_corners[8];
static float ambient_factor;
static double dt;
static double accumulator;
static shadow_atlas_t* shadow_atlas;
static vec3 cascade_corners[MAX_CASCADES][8];
static mat4 view_matrix;
static double fps = 60.0;

extern void input_process_keyboard(int32_t key, int32_t scancode, int32_t action, int32_t mods);
extern void input_process_mouse_position(double x_pos, double y_pos);
extern void input_process_mouse_button(int32_t button, int32_t action, int32_t mods);
extern void input_process_mouse_scroll(double x_offset, double y_offset);
extern void input_process_mouse_enter(int32_t entered);
extern void fixed_update(double dt);

extern void viewport_resized(int32_t width, int32_t height);

static struct {
	gfx_context *gfx;
	uint32_t command_count;
	render_command_t command_queue[MAX_QUEUE];
} state;


static void error_callback(int error, const char* description) {
	fprintf(stderr, "Error #%d :%s\n", error, description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	UNUSED_ARG(window);
	input_process_keyboard(key, scancode, action, mods);
}

static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
	UNUSED_ARG(window);
	if (inside_current_state) input_process_mouse_position(xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	UNUSED_ARG(window);
	input_process_mouse_button(button, action, mods);
}

static void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	UNUSED_ARG(window);
	input_process_mouse_scroll(xoffset, yoffset);
}

void renderer_process_input(void) {
	inside_current_state = glfwGetWindowAttrib(window, GLFW_HOVERED) ? 1 : 0;
	if (inside_current_state != inside_previous_state) {
		input_process_mouse_enter(inside_current_state);
		inside_previous_state = inside_current_state;
	}
	glfwPollEvents();
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	state.gfx->framebuffer_width = width;
	state.gfx->framebuffer_height = height;

	int win_w, win_h;
	glfwGetWindowSize(window, &win_w, &win_h);
	state.gfx->width  = win_w;
	state.gfx->height = win_h;

	glViewport(0, 0, width, height);
	viewport_resized(win_w, win_h);
}


gfx_context* gfx_context_init(app_config_t* cfg) {
	gfx_context* gc = malloc(sizeof(*gc));
	if (!gc) { perror("gfx_context_init OOM"); exit(-1); }
	gc->width  = cfg->width;
	gc->height = cfg->height;
	gc->fps    = cfg->fps;
	gc->title  = cfg->title;

	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); exit(-1); }

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	window = glfwCreateWindow(gc->width, gc->height, gc->title, NULL, NULL);
	if (!window) { fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); exit(-1); }

	glfwGetWindowSize(window, &gc->width, &gc->height);
	glfwGetFramebufferSize(window, &gc->framebuffer_width, &gc->framebuffer_height);

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, mouse_scroll_callback);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPos(window, gc->width / 2, gc->height / 2);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		fprintf(stderr, "Failed to initialize GLAD\n");
		glfwTerminate();
		exit(-1);
	}

	return gc;
}


void gfx_context_shutdown(gfx_context* ctx) {
	glfwTerminate();
	free(ctx);
}


int renderer_should_close(void) { return glfwWindowShouldClose(window); }


void renderer_loop_setup(void) {
	last_frame_time = glfwGetTime();
}


static void dt_update(void) {
	double current_frame_time = glfwGetTime();
	dt = (double) fmin(current_frame_time - last_frame_time, MAX_DT);
	last_frame_time = current_frame_time;
	if (dt > 0.001) fps = fps * 0.95 + (1.0 / dt) * 0.05;
}


double renderer_dt(void) { return dt; }


void renderer_fixed_update(void) {
	accumulator += dt;
	if (accumulator > (FIXED_STEP * 10)) accumulator = FIXED_STEP * 10;
	while (accumulator >= FIXED_STEP) {
		fixed_update(FIXED_STEP);
		accumulator -= FIXED_STEP;
	}
}


void renderer_begin_frame(void) {
	dt_update();
	// Open both 2D and 3D batches at the start of the frame so any push from
	// application_update lands in the right per-frame arena. Symmetric with
	// the 2D pipeline (Stage 22).
	batch_begin_frame();
	batch2d_begin_frame();
}


void renderer_end_frame(void) {
	glfwSwapBuffers(window);
}


// ---- 3D shader/scene uniform push (lit / unlit) --------------------------
//
// Stage 23: surface_color uniform is no-op (shader doesn't have it anymore)
// because the per-instance aColor attribute carries the baked color.
// The push function stays defensive — if a custom shader still has the
// uniform, we forward material->surface_color, but the built-in lit/unlit
// don't read it.

static void push_shader_data(material_t* mat, mat4 vp) {
	builtin_locations_t* loc = &mat->shader->locations;
	if (loc->vp >= 0)
		glUniformMatrix4fv(loc->vp, 1, GL_FALSE, (float*)&vp.m);
	if (loc->surface_color >= 0)
		glUniform3fv(loc->surface_color, 1, (float*)&mat->surface_color);
	if (loc->specular_color >= 0)
		glUniform3fv(loc->specular_color, 1, (float*)&mat->specular_color);
	if (loc->shininess >= 0)
		glUniform1f(loc->shininess, mat->shininess);
	if (loc->emissive_color >= 0)
		glUniform3fv(loc->emissive_color, 1, (float*)&mat->emissive_color);
	if (loc->opacity >= 0)
		glUniform1f(loc->opacity, mat->opacity);
	if (loc->diffuse_texture >= 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mat->diffuse_texture->id);
	}
	if (loc->normal_texture >= 0) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, mat->normal_texture->id);
	}
}


static void push_scene_data(material_t* mat) {
	builtin_locations_t* loc = &mat->shader->locations;

	if (loc->diffuse_texture >= 0) glUniform1i(loc->diffuse_texture, 0);
	if (loc->normal_texture  >= 0) glUniform1i(loc->normal_texture,  1);
	if (loc->ambient_factor  >= 0) glUniform1f(loc->ambient_factor, ambient_factor);

	uint32_t count = light_get_count();
	if (count == 0) return;
	light_t* light = light_get_table();

	if (loc->light_count     >= 0) glUniform1i(loc->light_count, count);
	if (loc->camera_position >= 0) glUniform3fv(loc->camera_position, 1, (float*)&camera_position);

	for (uint32_t i = 0; i < count; i++) {
		if (loc->light[i].light_type >= 0)
			glUniform1i(loc->light[i].light_type, light[i].type);
		if (loc->light[i].light_color >= 0)
			glUniform3fv(loc->light[i].light_color, 1, (float*)&light[i].color);
		if (loc->light[i].light_intensity >= 0)
			glUniform1f(loc->light[i].light_intensity, light[i].intensity);
		if (loc->light[i].light_direction >= 0)
			glUniform3fv(loc->light[i].light_direction, 1, (float*)&light[i].direction);
		if (loc->light[i].light_position >= 0)
			glUniform3fv(loc->light[i].light_position, 1, (float*)&light[i].position);
		if (loc->light[i].constant_attenuation >= 0)
			glUniform1f(loc->light[i].constant_attenuation, light[i].attenuation.constant);
		if (loc->light[i].linear_attenuation >= 0)
			glUniform1f(loc->light[i].linear_attenuation, light[i].attenuation.linear);
		if (loc->light[i].quadratic_attenuation >= 0)
			glUniform1f(loc->light[i].quadratic_attenuation, light[i].attenuation.quadratic);
		if (loc->light[i].cone_inner_cutoff >= 0)
			glUniform1f(loc->light[i].cone_inner_cutoff, light[i].cone.inner_cutoff);
		if (loc->light[i].cone_outer_cutoff >= 0)
			glUniform1f(loc->light[i].cone_outer_cutoff, light[i].cone.outer_cutoff);
	}

	if (loc->shadow_atlas >= 0 && shadow_atlas) {
		glUniform1i(loc->shadow_atlas, 2);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, shadow_atlas->texture_id);
	}

	for (uint32_t i = 0; i < count; i++) {
		if (light[i].cascade_tiles[0] >= 0) {
			shadow_tile_t* tile = &shadow_atlas->tiles[light[i].cascade_tiles[0]];
			if (loc->light_vp[i]    >= 0) glUniformMatrix4fv(loc->light_vp[i], 1, GL_FALSE, (float*)&tile->vp.m);
			if (loc->shadow_rect[i] >= 0) glUniform4fv(loc->shadow_rect[i], 1, (float*)&tile->rect);
		} else {
			if (loc->shadow_rect[i] >= 0) {
				vec4 zero = {0.0f, 0.0f, 0.0f, 0.0f};
				glUniform4fv(loc->shadow_rect[i], 1, (float*)&zero);
			}
		}
	}

	if (loc->cascade_count >= 0) {
		int32_t csm_count = 0;
		shadow_atlas_t* atlas = shadow_atlas_get();
		for (uint32_t i = 0; i < count; i++) {
			if (light[i].type == LIGHT_DIRECTIONAL && light[i].cascade_count > 1) {
				csm_count = light[i].cascade_count;
				for (int32_t c = 0; c < csm_count; c++) {
					int32_t tile_idx = light[i].cascade_tiles[c];
					if (tile_idx < 0) continue;
					shadow_tile_t* tile = &shadow_atlas->tiles[tile_idx];
					if (loc->cascade_vp[c]    >= 0) glUniformMatrix4fv(loc->cascade_vp[c], 1, GL_FALSE, (float*)&tile->vp.m);
					if (loc->cascade_rect[c]  >= 0) glUniform4fv(loc->cascade_rect[c], 1, (float*)&tile->rect);
					if (loc->cascade_splits[c]>= 0) glUniform1f(loc->cascade_splits[c], atlas->split_distances[c + 1]);
				}
				if (loc->shadow_rect[i] >= 0) {
					vec4 zero = {0.0f, 0.0f, 0.0f, 0.0f};
					glUniform4fv(loc->shadow_rect[i], 1, (float*)&zero);
				}
				break;
			}
		}
		glUniform1i(loc->cascade_count, csm_count);
	}

	if (loc->view_matrix >= 0)
		glUniformMatrix4fv(loc->view_matrix, 1, GL_FALSE, (float*)&view_matrix.m);
}


// ---- Per-mesh instance VBO upload ----------------------------------------
//
// Splits the AoS batch_instance_t array into the three SoA streams the GPU
// expects (transform / uv_rect / color), then uploads each into the mesh's
// instance VBOs. The mesh owns the VBOs and the VAO bindings — we just push
// data into them.
//
// At MAX_OBJECTS3D = 10000 the SoA split copy is well under a millisecond
// per frame even at full saturation, and it lets the lit/unlit shaders read
// each attribute from a tightly-packed buffer (GPU prefers SoA for streamed
// vertex/instance data).
//
// Scratch buffers are static module-local — sized to BATCH_MAX_RECORDS (10000)
// of each type. Lifetime spans multiple frames; the buffers are overwritten
// every upload.
#define UPLOAD_MAX_INSTANCES 10000

static mat4    upload_transforms[UPLOAD_MAX_INSTANCES];
static vec4    upload_uv_rects  [UPLOAD_MAX_INSTANCES];
static color_t upload_colors    [UPLOAD_MAX_INSTANCES];

static void upload_instance_streams(mesh_t* mesh, const batch_instance_t* src, uint32_t count) {
	if (count > UPLOAD_MAX_INSTANCES) count = UPLOAD_MAX_INSTANCES;

	for (uint32_t i = 0; i < count; i++) {
		upload_transforms[i] = src[i].transform;
		upload_uv_rects  [i] = src[i].uv_rect;
		upload_colors    [i] = src[i].color;
	}

	glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_vbo_transform);
	glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * count, upload_transforms, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_vbo_uv_rect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec4) * count, upload_uv_rects, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->instance_vbo_color);
	glBufferData(GL_ARRAY_BUFFER, sizeof(color_t) * count, upload_colors, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


// ---- 3D Opaque pass ------------------------------------------------------
//
// Iterates the pre-sorted opaque groups produced by batch_end_frame. Each
// group is one glDrawElementsInstanced call. Shader is rebound only when
// the next group's material->shader differs from the current one — within a
// shader, we still push per-material uniforms (specular, shininess, etc.)
// because materials inside the same shader can differ.
static void opaque_pass(void) {
	uint32_t group_count = batch_get_opaque_group_count();
	if (group_count == 0) return;

	const batch_group_t*    groups    = batch_get_opaque_groups();
	const batch_instance_t* instances = batch_get_opaque_instances();

	shader_t* current_shader = NULL;
	for (uint32_t i = 0; i < group_count; i++) {
		const batch_group_t* g = &groups[i];

		if (g->material->shader != current_shader) {
			current_shader = g->material->shader;
			glUseProgram(current_shader->program_id);
			push_scene_data(g->material);
		}

		push_shader_data(g->material, g->vp);

		upload_instance_streams(g->mesh, &instances[g->instance_offset], g->instance_count);

		glBindVertexArray(g->mesh->vao);
		glDrawElementsInstanced(
			GL_TRIANGLES,
			g->mesh->index_count,
			GL_UNSIGNED_INT,
			0,
			g->instance_count
		);
	}
}


// ---- 3D Transparent pass -------------------------------------------------
//
// The transparent registry already comes back-to-front sorted by the batch's
// depth-bucket key (see build_sort_key_transparent). We just iterate, set
// blend state once at pass entry, and emit one draw per group.
//
// Group instance_count is typically 1 because each transparent object has a
// unique depth bucket — so the upload-then-draw loop is effectively
// per-object, same as the pre-Stage-23 behavior.
static void transparent_pass(void) {
	uint32_t group_count = batch_get_transparent_group_count();
	if (group_count == 0) return;

	const batch_group_t*    groups    = batch_get_transparent_groups();
	const batch_instance_t* instances = batch_get_transparent_instances();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	shader_t* current_shader = NULL;
	for (uint32_t i = 0; i < group_count; i++) {
		const batch_group_t* g = &groups[i];

		if (g->material->shader != current_shader) {
			current_shader = g->material->shader;
			glUseProgram(current_shader->program_id);
			push_scene_data(g->material);
		}

		push_shader_data(g->material, g->vp);

		upload_instance_streams(g->mesh, &instances[g->instance_offset], g->instance_count);

		glBindVertexArray(g->mesh->vao);
		glDrawElementsInstanced(
			GL_TRIANGLES,
			g->mesh->index_count,
			GL_UNSIGNED_INT,
			0,
			g->instance_count
		);
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}


// ---- Shadow pass ---------------------------------------------------------
//
// Re-uses the upload_instance_streams path (same SoA buffers as the opaque
// pass — they live until the next upload, no harm in stomping them per-tile
// because the shadow pass runs before the opaque pass).
static void shadow_pass(void) {
	glBindFramebuffer(GL_FRAMEBUFFER, shadow_atlas->fbo);
	glClear(GL_DEPTH_BUFFER_BIT);

	shader_t* shader = shader_get_shadow();
	glUseProgram(shader->program_id);
	if (shader->locations.diffuse_texture >= 0)
		glUniform1i(shader->locations.diffuse_texture, 0);

	light_t* light = light_get_table();
	uint32_t count = light_get_count();

	const batch_group_t*    op_groups    = batch_get_opaque_groups();
	const batch_instance_t* op_instances = batch_get_opaque_instances();
	uint32_t op_count = batch_get_opaque_group_count();

	const batch_group_t*    tr_groups    = batch_get_transparent_groups();
	const batch_instance_t* tr_instances = batch_get_transparent_instances();
	uint32_t tr_count = batch_get_transparent_group_count();

	for (uint32_t i = 0; i < count; i++) {
		if (light[i].cascade_count == 0) continue;

		for (int32_t c = 0; c < light[i].cascade_count; c++) {
			int32_t tile_idx = light[i].cascade_tiles[c];
			if (tile_idx < 0) continue;

			vec3* corners = (light[i].type == LIGHT_DIRECTIONAL)
				? cascade_corners[c]
				: active_frustum_corners;

			shadow_map_update(&light[i], corners, tile_idx);

			shadow_tile_t* tile = &shadow_atlas->tiles[tile_idx];
			glViewport(tile->x, tile->y, tile->size, tile->size);

			if (shader->locations.vp >= 0)
				glUniformMatrix4fv(shader->locations.vp, 1, GL_FALSE, (float*)&tile->vp.m);

			// Opaque casters
			for (uint32_t j = 0; j < op_count; j++) {
				const batch_group_t* g = &op_groups[j];
				upload_instance_streams(g->mesh, &op_instances[g->instance_offset], g->instance_count);
				glBindVertexArray(g->mesh->vao);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, g->material->diffuse_texture->id);
				glDrawElementsInstanced(GL_TRIANGLES, g->mesh->index_count,
				                        GL_UNSIGNED_INT, 0, g->instance_count);
			}

			// Transparent casters (those flagged cast_shadow)
			for (uint32_t j = 0; j < tr_count; j++) {
				const batch_group_t* g = &tr_groups[j];
				if (!g->material->cast_shadow) continue;
				upload_instance_streams(g->mesh, &tr_instances[g->instance_offset], g->instance_count);
				glBindVertexArray(g->mesh->vao);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, g->material->diffuse_texture->id);
				glDrawElementsInstanced(GL_TRIANGLES, g->mesh->index_count,
				                        GL_UNSIGNED_INT, 0, g->instance_count);
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, state.gfx->framebuffer_width, state.gfx->framebuffer_height);
}


// ---- 2D Overlay pass — unchanged from Stage 22 ---------------------------
//
// (Body identical to the Stage 22 implementation — kept here verbatim. The
// only Stage 23 effect on this pass is shader-side: the sprite shader now
// linearizes frag_color before multiplying with the texture.)
static void overlay_pass(void) {
	uint32_t group_count = batch2d_get_group_count();
	if (group_count == 0) return;

	const batch2d_group_t* groups = batch2d_get_groups();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	shader_t* shader = shader_get_sprite();
	glUseProgram(shader->program_id);

	GLint loc_view       = glGetUniformLocation(shader->program_id, "view");
	GLint loc_projection = glGetUniformLocation(shader->program_id, "projection");
	GLint loc_diffuse    = glGetUniformLocation(shader->program_id, "diffuse_texture");

	if (loc_diffuse >= 0) glUniform1i(loc_diffuse, 0);

	batch2d_bind_quad_vao();
	glActiveTexture(GL_TEXTURE0);

	camera2d_t*  current_camera     = NULL;
	blend_mode_t current_blend      = (blend_mode_t)-1;
	uint32_t     current_texture_id = 0;

	GLuint vbo_model   = batch2d_get_instance_vbo_model();
	GLuint vbo_uv_rect = batch2d_get_instance_vbo_uv_rect();
	GLuint vbo_color   = batch2d_get_instance_vbo_color();

	for (uint32_t i = 0; i < group_count; i++) {
		const batch2d_group_t* g = &groups[i];

		if (g->camera != current_camera) {
			current_camera = g->camera;
			mat4 view = camera2d_get_view_matrix(current_camera);
			mat4 proj = camera2d_get_projection_matrix(current_camera);
			if (loc_view       >= 0) glUniformMatrix4fv(loc_view,       1, GL_FALSE, view.m);
			if (loc_projection >= 0) glUniformMatrix4fv(loc_projection, 1, GL_FALSE, proj.m);
		}

		if (g->blend_mode != current_blend) {
			current_blend = g->blend_mode;
			switch (current_blend) {
				case BLEND_NORMAL:   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
				case BLEND_ADDITIVE: glBlendFunc(GL_SRC_ALPHA, GL_ONE);                 break;
				case BLEND_MULTIPLY: glBlendFunc(GL_DST_COLOR, GL_ZERO);                break;
				default:             glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
			}
		}

		if (g->texture_id != current_texture_id) {
			current_texture_id = g->texture_id;
			glBindTexture(GL_TEXTURE_2D, current_texture_id);
		}

		size_t off_model   = (size_t)g->instance_offset * sizeof(mat4);
		size_t off_uv_rect = (size_t)g->instance_offset * sizeof(vec4);
		size_t off_color   = (size_t)g->instance_offset * sizeof(color_t);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_model);
		for (int c = 0; c < 4; c++) {
			glVertexAttribPointer(6 + c, 4, GL_FLOAT, GL_FALSE,
				sizeof(mat4),
				(void*)(off_model + sizeof(float) * 4 * c));
		}

		glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_rect);
		glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE,
			sizeof(vec4), (void*)off_uv_rect);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_color);
		glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE,
			sizeof(color_t), (void*)off_color);

		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, g->instance_count);
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}


void renderer_draw(void) {
	glClearColor(0.4f, 0.45f, 0.5f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	for (size_t i = 0; i < state.command_count; i++) {
		render_command_t cmd = state.command_queue[i];
		switch (state.command_queue[i].type) {
			case R_CMD_CLEAR:
				glClearColor(cmd.clear.color.r, cmd.clear.color.g, cmd.clear.color.b, cmd.clear.color.a);
				glClear(GL_COLOR_BUFFER_BIT);
				break;
			default:
				break;
		}
	}
	state.command_count = 0;

	// Stage 23: collect retained 3D objects into the batch (mirroring the 2D
	// collect step). After this, batch_end_frame produces the sorted group
	// arrays that all three 3D passes (shadow / opaque / transparent) consume.
	object3d_collect_into_batch();
	batch_end_frame();

	shadow_pass();
	opaque_pass();
	transparent_pass();

	// 2D overlay — the 2D collect step happens here, mirroring the 3D one.
	object2d_collect_into_batch();
	batch2d_end_frame();
	overlay_pass();
}


double renderer_fps(void) { return fps; }


void renderer_init(gfx_context* ctx) {
	state.gfx = ctx;
	state.command_count = 0;
	last_frame_time = glfwGetTime();
	ambient_factor = DEFAULT_AMBIENT_FACTOR;

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_FRAMEBUFFER_SRGB);

	batch_init();
	shader_init();
	texture_init();
	light_init();
	material_init();
	material2d_init();
	batch2d_init();
	text_init();

	// Stage 23: object3d pool init mirrors object2d_init in cubit.c.
	// Initialized here too so the pool exists before the first frame's
	// collect_into_batch — even though no objects are created yet.
	object3d_init();
}


void renderer_shutdown(void) {
	object3d_shutdown();
	text_shutdown();
	batch2d_shutdown();
	material2d_shutdown();
	material_shutdown();
	light_shutdown();
	texture_shutdown();
	shader_shutdown();
	batch_shutdown();
}


void renderer_submit(render_command_t* cmd) {
	if (state.command_count >= MAX_QUEUE) {
		fprintf(stderr, "Warning command_queue maxed out\n");
		return;
	}
	size_t i = state.command_count;
	state.command_queue[i].type = cmd->type;
	switch (cmd->type) {
		case R_CMD_CLEAR:
			state.command_queue[i].clear.color = cmd->clear.color;
			state.command_queue[i].clear.flags = cmd->clear.flags;
			break;
		case R_CMD_DRAW:
		case R_CMD_DRAW_INSTANCED:
			break;
		default:
			return;
	}
	state.command_count++;
}


// ---- BACKEND OPENGL HANDLING ---------------------------------------------

static void push_buffer(uint32_t* vbo, void* data, uint32_t vertex_count, uint32_t size, uint32_t loc) {
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * size * sizeof(float), data, GL_STATIC_DRAW);
	glVertexAttribPointer(loc, size, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(loc);
}


void backend_mesh_new(mesh_t* m, void* pos, void* nor, void* uv, void* tg, uint32_t* indices, uint32_t vertex_count, uint32_t index_count) {
	m->vertex_count = vertex_count;
	m->index_count  = index_count;
	m->vbo_position = 0;
	m->vbo_normal   = 0;
	m->vbo_uv       = 0;

	glGenVertexArrays(1, &m->vao);
	glBindVertexArray(m->vao);

	if (pos) push_buffer(&m->vbo_position, pos, vertex_count, 3, LOC_POSITION);
	if (nor) push_buffer(&m->vbo_normal,   nor, vertex_count, 3, LOC_NORMAL);
	if (uv)  push_buffer(&m->vbo_uv,       uv,  vertex_count, 2, LOC_UV);
	if (tg)  push_buffer(&m->vbo_tangent,  tg,  vertex_count, 3, LOC_TANGENT);

	glGenBuffers(1, &m->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), indices, GL_STATIC_DRAW);

	// Instance model matrix (locations 6..9)
	glGenBuffers(1, &m->instance_vbo_transform);
	glBindBuffer(GL_ARRAY_BUFFER, m->instance_vbo_transform);
	uint32_t loc = LOC_MODEL;
	for (uint32_t i = 0; i < 4; i++) {
		glVertexAttribPointer(loc + i, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void*)(i * sizeof(vec4)));
		glEnableVertexAttribArray(loc + i);
		glVertexAttribDivisor(loc + i, 1);
	}

	// Instance uv_rect (location 10)
	glGenBuffers(1, &m->instance_vbo_uv_rect);
	glBindBuffer(GL_ARRAY_BUFFER, m->instance_vbo_uv_rect);
	glVertexAttribPointer(LOC_UV_RECT, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void*)0);
	glEnableVertexAttribArray(LOC_UV_RECT);
	glVertexAttribDivisor(LOC_UV_RECT, 1);

	// Instance color (location 11) — newly used in Stage 23 for per-instance tint
	glGenBuffers(1, &m->instance_vbo_color);
	glBindBuffer(GL_ARRAY_BUFFER, m->instance_vbo_color);
	glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(color_t), (void*)0);
	glEnableVertexAttribArray(11);
	glVertexAttribDivisor(11, 1);

	glBindVertexArray(0);
}


void backend_mesh_destroy(mesh_t* m) {
	glDeleteVertexArrays(1, &m->vao);
	glDeleteBuffers(1, &m->instance_vbo_color);
	glDeleteBuffers(1, &m->instance_vbo_uv_rect);
	glDeleteBuffers(1, &m->instance_vbo_transform);
	glDeleteBuffers(1, &m->vbo_position);
	glDeleteBuffers(1, &m->vbo_normal);
	glDeleteBuffers(1, &m->vbo_uv);
	glDeleteBuffers(1, &m->vbo_tangent);
	glDeleteBuffers(1, &m->ebo);
}


// ---- BACKEND PLATFORM HANDLING -------------------------------------------

void backend_shutdown(void) {
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void backend_set_mouse_mode(int32_t mode) {
	switch (mode) {
		case MOUSE_NORMAL:
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case MOUSE_RAW:
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported())
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			break;
		default:
			break;
	}
}

void backend_get_window_size(int32_t* w, int32_t* h) {
	if (w) *w = state.gfx->width;
	if (h) *h = state.gfx->height;
}

void backend_get_framebuffer_size(int32_t* w, int32_t* h) {
	if (w) *w = state.gfx->framebuffer_width;
	if (h) *h = state.gfx->framebuffer_height;
}


void backend_texture_new(texture_t* t, unsigned char* raw_pixel) {
	glGenTextures(1, &t->id);
	glBindTexture(GL_TEXTURE_2D, t->id);

	switch (t->min_filter) {
		case NEAREST: glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); break;
		case LINEAR:  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); break;
		default: break;
	}
	switch (t->mag_filter) {
		case NEAREST: glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); break;
		case LINEAR:  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  break;
		default: break;
	}
	switch (t->s_wrap) {
		case REPEAT: glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); break;
		case CLAMP:  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); break;
		default: break;
	}
	switch (t->t_wrap) {
		case REPEAT: glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); break;
		case CLAMP:  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); break;
		default: break;
	}

	switch (t->channels) {
		case 3:
			if (t->type == COLOR_TEXTURE)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, t->width, t->height, 0, GL_RGB, GL_UNSIGNED_BYTE, raw_pixel);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, t->width, t->height, 0, GL_RGB, GL_UNSIGNED_BYTE, raw_pixel);
			break;
		case 4:
			if (t->type == COLOR_TEXTURE)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, t->width, t->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw_pixel);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t->width, t->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw_pixel);
			break;
		default: break;
	}

	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}


void backend_texture_destroy(texture_t* t) {
	glDeleteTextures(1, &t->id);
}


void backend_ambient_factor_set(float f) { ambient_factor = f; }
float backend_ambient_factor_get(void)   { return ambient_factor; }


void backend_shadow_atlas_new(shadow_atlas_t* sa) {
	glGenTextures(1, &sa->texture_id);
	glBindTexture(GL_TEXTURE_2D, sa->texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, sa->atlas_size, sa->atlas_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

	glGenFramebuffers(1, &sa->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, sa->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sa->texture_id, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	shadow_atlas = sa;
}


void backend_shadow_atlas_destroy(shadow_atlas_t* sa) {
	glDeleteTextures(1, &sa->texture_id);
	glDeleteFramebuffers(1, &sa->fbo);
}


void backend_set_camera_position(vec3 pos) { camera_position = pos; }
void backend_set_view_matrix(mat4 v)        { view_matrix = v; }

void backend_set_active_frustum_corners(vec3* corners) {
	memcpy((void*)active_frustum_corners, (const void*)corners, 8 * sizeof(vec3));
}

void backend_set_cascade_corners(uint32_t cascade, vec3* corners) {
	memcpy(cascade_corners[cascade], corners, 8 * sizeof(vec3));
}
