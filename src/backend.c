// backend.c

#include "backend.h"
#include "batch.h"
#include "material.h"
#include "shader.h"
#include "light.h"
#include "mesh.h"
#include "texture.h"
#include "shadow.h"

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
	int fps;
	const char* title;
};

static GLFWwindow *window;
static int32_t inside_previous_state  = 0;
static int32_t inside_current_state  = 0;
static double last_frame_time = 0.0;
static vec3 camera_position; // Per frame camera position
static vec3 active_frustum_corners[8]; // The active camea frustum corners
static float ambient_factor; // Per scene ambient factor defaulted 0.1f
static double dt;
static double accumulator;

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
	inside_current_state = glfwGetWindowAttrib(window, GLFW_HOVERED)? 1: 0;
	if (inside_current_state != inside_previous_state) {
		input_process_mouse_enter(inside_current_state);
		inside_previous_state = inside_current_state;
	}
    glfwPollEvents();
}

/*
 * glfw: whenever the window size changed (by OS or user resize)
 * this callback function executes
 */
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	UNUSED_ARG(window);
    glViewport(0, 0, width, height);
	viewport_resized(width, height);
}


gfx_context *gfx_context_init(app_config_t* cfg) {
	gfx_context *gc = malloc(sizeof(*gc));
	if (gc == NULL) {
		perror("gfx_context_int: out of memory creating context");
		exit(-1);
	}
	gc->width = cfg->width;
	gc->height = cfg->height;
	gc->fps = cfg->fps;
	gc->title = cfg->title;

	glfwSetErrorCallback(error_callback);

	// Initialize GLFW
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		exit(-1);
	}
	// Configure window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// Window creation
    window = glfwCreateWindow(gc->width, gc->height, gc->title, NULL, NULL);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        exit(-1);
    }

	// Context
    glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // enable vsync

	// Set iintial mouse state
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Register key input callback
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, mouse_scroll_callback);
	//glfwSetCursorEnterCallback(window, mouse_enter_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// Set initial mouse position
	glfwSetCursorPos(window, gc->width/2, gc->height/2);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
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


int renderer_should_close(void){
	return glfwWindowShouldClose(window);
}

/* Setup needed before entering
 * the infinite loop */
void renderer_loop_setup(void) {
    // reset last frame time
    last_frame_time = glfwGetTime();
}

static void dt_update(void) {
	double current_frame_time = glfwGetTime();
	dt = (double) fmin(current_frame_time - last_frame_time, MAX_DT);
	last_frame_time = current_frame_time;
}

double renderer_dt(void) {
	return dt;
}

void renderer_fixed_update(void) {
    accumulator += dt;
    if (accumulator > (FIXED_STEP * 10))
        accumulator = FIXED_STEP * 10;
    while (accumulator >= FIXED_STEP) {
        fixed_update(FIXED_STEP);
        accumulator -= FIXED_STEP;
    }
}

void renderer_begin_frame(void){
    dt_update();
	batch_cleanup();
}


void renderer_end_frame(void) {
    glfwSwapBuffers(window);
}

static void push_transparent_scene_data(transparent_entry_t *e) {
    builtin_locations_t* loc = &e->material->shader->locations;

    if (loc->diffuse_texture >= 0)
        glUniform1i(loc->diffuse_texture, 0);
    if (loc->normal_texture >= 0)
        glUniform1i(loc->normal_texture, 1);

    if (loc->ambient_factor >= 0)
        glUniform1f(loc->ambient_factor, ambient_factor);

    uint32_t count = light_get_count();
    if (count == 0) return;

    light_t* light = light_get_table();

    if (loc->light_count >= 0)
        glUniform1i(loc->light_count, count);
    if (loc->camera_position >= 0)
        glUniform3fv(loc->camera_position, 1, (float*)&camera_position);

    int32_t shadow_slot = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (light[i].shadow_map != NULL && shadow_slot < MAX_SHADOW_MAPS) {
            light[i].shadow_index = shadow_slot++;
        } else {
            light[i].shadow_index = -1;
        }
    }

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
        if (loc->light[i].shadow_index >= 0)
            glUniform1i(loc->light[i].shadow_index, light[i].shadow_index);
    }

    for (uint32_t i = 0; i < count; i++) {
        if (light[i].shadow_index >= 0 && light[i].shadow_map) {
            int32_t idx = light[i].shadow_index;
            if (loc->shadow_map[idx] >= 0) {
                glUniform1i(loc->shadow_map[idx], 2 + idx);
                glActiveTexture(GL_TEXTURE2 + idx);
                glBindTexture(GL_TEXTURE_2D, light[i].shadow_map->id);
            }
            if (loc->light_vp[idx] >= 0)
                glUniformMatrix4fv(loc->light_vp[idx], 1, GL_FALSE, (float*)&light[i].shadow_map->vp.m);
        }
    }
}

static void push_transparent_shader_data(transparent_entry_t *e) {
    builtin_locations_t* loc = &e->material->shader->locations;
    if (loc->vp >= 0)
        glUniformMatrix4fv(loc->vp, 1, GL_FALSE, (float*)&e->vp.m);
    if (loc->surface_color >= 0)
        glUniform3fv(loc->surface_color, 1, (float*)&e->material->surface_color);
    if (loc->specular_color >= 0)
        glUniform3fv(loc->specular_color, 1, (float*)&e->material->specular_color);
    if (loc->shininess >= 0)
        glUniform1f(loc->shininess, e->material->shininess);
    if (loc->emissive_color >= 0)
        glUniform3fv(loc->emissive_color, 1, (float*)&e->material->emissive_color);
    if (loc->opacity >= 0)
        glUniform1f(loc->opacity, e->material->opacity);
    if (loc->diffuse_texture >= 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, e->material->diffuse_texture->id);
    }
    if (loc->normal_texture >= 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, e->material->normal_texture->id);
    }
}

static void push_shader_data(batch_registry_entry_t *e) {
	builtin_locations_t* loc = &e->material->shader->locations;
	if (loc->vp >= 0)
		glUniformMatrix4fv(loc->vp, 1, GL_FALSE, (float*)&e->vp.m);
	if (loc->surface_color >= 0)
		glUniform3fv(loc->surface_color, 1, (float*)&e->material->surface_color);
	if (loc->specular_color >= 0)
	    glUniform3fv(loc->specular_color, 1, (float*)&e->material->specular_color);
	if (loc->shininess >= 0)
	    glUniform1f(loc->shininess, e->material->shininess);
	if (loc->emissive_color >= 0)
	    glUniform3fv(loc->emissive_color, 1, (float*)&e->material->emissive_color);
    if (loc->opacity >= 0)
        glUniform1f(loc->opacity, e->material->opacity);
	if (loc->diffuse_texture >= 0) {
		glActiveTexture(GL_TEXTURE0);  // Activate texture at slot 0
		glBindTexture(GL_TEXTURE_2D, e->material->diffuse_texture->id);
	}
	if (loc->normal_texture >= 0) {
		glActiveTexture(GL_TEXTURE1);  // Activate texture at slot 1
		glBindTexture(GL_TEXTURE_2D, e->material->normal_texture->id);
	}
}

static void push_scene_data(batch_registry_entry_t *e) {
	// get location
	builtin_locations_t* loc = &e->material->shader->locations;

	// Texture
	if (loc->diffuse_texture >= 0)
		glUniform1i(loc->diffuse_texture, 0);
	if (loc->normal_texture >= 0)
		glUniform1i(loc->normal_texture, 1);

	// Ambient factor
	if (loc->ambient_factor >= 0)
		glUniform1f(loc->ambient_factor, ambient_factor);

	uint32_t count = light_get_count();
	if (count == 0) return;

	light_t* light = light_get_table();

	if (loc->light_count >= 0)
		glUniform1i(loc->light_count, count);
	if (loc->camera_position >= 0)
		glUniform3fv(loc->camera_position, 1, (float*)&camera_position);

	// Assign shadow slots BEFORE pushing light data
	int32_t shadow_slot = 0;
	for (uint32_t i = 0; i < count; i++) {
		if (light[i].shadow_map != NULL && shadow_slot < MAX_SHADOW_MAPS) {
			light[i].shadow_index = shadow_slot++;
		} else {
			light[i].shadow_index = -1;
		}
	}

	// Now push light data (shadow_index is already correct)
	for (uint32_t i = 0; i < count; i++) {
		if (loc->light[i].light_type >= 0)
			glUniform1i(loc->light[i].light_type, light[i].type);
		if (loc->light[i].light_color >= 0)
			glUniform3fv(loc->light[i].light_color, 1, (float*)&light[i].color );
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
		if (loc->light[i].shadow_index >= 0)
			glUniform1i(loc->light[i].shadow_index, light[i].shadow_index);
	}

	// Bind shadow map textures and push VP matrices
	for (uint32_t i = 0; i < count; i++) {
		if (light[i].shadow_index >= 0 && light[i].shadow_map) {
			int32_t idx = light[i].shadow_index;
			if (loc->shadow_map[idx] >= 0) {
				glUniform1i(loc->shadow_map[idx], 2 + idx);
				glActiveTexture(GL_TEXTURE2 + idx);
				glBindTexture(GL_TEXTURE_2D, light[i].shadow_map->id);
			}
			if (loc->light_vp[idx] >= 0)
				glUniformMatrix4fv(loc->light_vp[idx], 1, GL_FALSE, (float*)&light[i].shadow_map->vp.m);
		}
	}
}

/* TRANSPARENT PASS */
static void transparent_pass(void) {
    // Order baack to front
    batch_sort_transparent();
    // Enable blending of the color
    glEnable(GL_BLEND);
    // Calulate the blending with a classic formula
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Depth wite-off: read but not write
    glDepthMask(GL_FALSE);
    // Loop trough the transparent object
    shader_t* current_shader = NULL;
    for (uint32_t i = 0; i < batch_transparent_size(); i++) {
        transparent_entry_t e = batch_get_transparent_entry(i);
        if (current_shader != e.material->shader) {
            current_shader = e.material->shader;
            glUseProgram(current_shader->program_id);
            push_transparent_scene_data(&e);
        }

        push_transparent_shader_data(&e);

        glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_transform);
        glBufferData(GL_ARRAY_BUFFER, sizeof(mat4), &e.transform, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_uv_rect);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec4), &e.uv_rect, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(e.mesh->vao);
        //glDrawElements(GL_TRIANGLES, e.mesh->index_count, GL_UNSIGNED_INT, 0);
        glDrawElementsInstanced(
            GL_TRIANGLES,
            e.mesh->index_count,
            GL_UNSIGNED_INT,
            0,
            1
        );
    }
    // Restore depth write
    glDepthMask(GL_TRUE);
    // Restore blending off
    glDisable(GL_BLEND);
}

/* SHADOW PASS */
static void shadow_pass(void) {
	light_t* light = light_get_table();
	for (uint32_t i = 0; i < light_get_count(); i++) {
		shadow_map_t *sm = light[i].shadow_map;
		if (sm) {
			shadow_map_update(sm, &light[i], active_frustum_corners);
			glBindFramebuffer(GL_FRAMEBUFFER, sm->fbo);
			glViewport(0, 0, sm->size, sm->size);
			glClear(GL_DEPTH_BUFFER_BIT);
			shader_t *shader = shader_get_shadow();
			glUseProgram(shader->program_id);

            // Pass the VP
			if (shader->locations.vp >= 0)
				glUniformMatrix4fv(shader->locations.vp, 1, GL_FALSE, (float*)&sm->vp.m);

            // Pass the diffuse texture
            if (shader->locations.diffuse_texture >=0)
                glUniform1i(shader->locations.diffuse_texture, 0);


            // Shadow cast for opaque objects
			for (uint32_t j = 0; j < batch_size(); j++) {
				batch_registry_entry_t e = batch_get_entry(j);
                // TRANSFORM
				glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_transform);
				glBufferData(
					GL_ARRAY_BUFFER,
					sizeof(mat4) * e.count,
					e.transforms,
					GL_DYNAMIC_DRAW
				);
                //UV RECT
                glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_uv_rect);
				glBufferData(
					GL_ARRAY_BUFFER,
					sizeof(vec4) * e.count,
					e.uv_rect,
					GL_DYNAMIC_DRAW
				);

				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindVertexArray(e.mesh->vao);

                // binda texture
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, e.material->diffuse_texture->id);

				glDrawElementsInstanced(
					GL_TRIANGLES,
					e.mesh->index_count,
					GL_UNSIGNED_INT,
					0,
					e.count
				);
			}

            // Shadow cast for transparent objects
            for (uint32_t i = 0; i < batch_transparent_size(); i++) {
                transparent_entry_t e = batch_get_transparent_entry(i);
                // Skipp shadow
                if (!e.material->cast_shadow) continue;

                glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_transform);
                glBufferData(GL_ARRAY_BUFFER, sizeof(mat4), &e.transform, GL_DYNAMIC_DRAW);

                glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_uv_rect);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vec4), &e.uv_rect, GL_DYNAMIC_DRAW);

                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(e.mesh->vao);

                // binda texture
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, e.material->diffuse_texture->id);

                glDrawElementsInstanced(
                    GL_TRIANGLES,
                    e.mesh->index_count,
                    GL_UNSIGNED_INT,
                    0,
                    1
                );
            }

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, state.gfx->width, state.gfx->height);
		}
	}
}


void renderer_draw(void) {
	glClearColor(0.4, 0.45, 0.5, 1.0); // Default background
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);


    // Clean
	for (size_t i = 0; i < state.command_count; i++){
		render_command_t cmd = state.command_queue[i];
		switch(state.command_queue[i].type) {
			case R_CMD_CLEAR:
				glClearColor(cmd.clear.color.r, cmd.clear.color.g, cmd.clear.color.b, cmd.clear.color.a);
    			glClear(GL_COLOR_BUFFER_BIT);
				break;
			default:
				;
				break;
		}
	}
	state.command_count = 0;

    // Shadow Pass
	shadow_pass();

    // Opaque Pass
	shader_t* current_shader = NULL;
	for (uint32_t i = 0; i < batch_size(); i++) {
		batch_registry_entry_t e = batch_get_entry(i);
		if (current_shader != e.material->shader) {
			current_shader = e.material->shader;
			glUseProgram(current_shader->program_id);
			push_scene_data(&e);
		}

		push_shader_data(&e);
        //TRANSFORM
		glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_transform);
		glBufferData(
			GL_ARRAY_BUFFER,
			sizeof(mat4) * e.count,
			e.transforms,
			GL_DYNAMIC_DRAW
		);
        //UV RECT
        glBindBuffer(GL_ARRAY_BUFFER, e.mesh->instance_vbo_uv_rect);
        glBufferData(
            GL_ARRAY_BUFFER,
            sizeof(vec4) * e.count,
            e.uv_rect,
            GL_DYNAMIC_DRAW
        );
        glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(e.mesh->vao);
		glDrawElementsInstanced(
			GL_TRIANGLES,
			e.mesh->index_count,
			GL_UNSIGNED_INT,
			0,
			e.count
		);
	};

    // Transparent Pass
    transparent_pass();
}

void renderer_init(gfx_context* ctx) {
	state.gfx = ctx;
	state.command_count = 0;
	last_frame_time = glfwGetTime();
	ambient_factor = DEFAULT_AMBIENT_FACTOR;

	// Enable Depth Testing
	// This ensures pixels closer to the camera obscure those further away.
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Enable Backface Culling
	// This tells OpenGL NOT to draw the inside faces of the cube, saving performance.
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);  // Cull the back faces
	glFrontFace(GL_CCW);  // Counter-Clockwise is the "front" (matches the corrected array)
    glEnable(GL_FRAMEBUFFER_SRGB); // Gamma Correction

	// batch registry init
	batch_init();
	shader_init();
	texture_init();
	light_init();
	material_init();
}


void renderer_shutdown(void) {
	material_shutdown();
	light_shutdown();
	texture_shutdown();
	shader_shutdown();
	batch_shutdown();
}


void renderer_submit(render_command_t *cmd) {
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
	state.command_count ++;
}


/*******************************************************************************
 * 	BACKEND OPENGL HANDLING
 *******************************************************************************/
static void push_buffer(uint32_t* vbo, void* data, uint32_t vertex_count, uint32_t size, uint32_t loc) {
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * size * sizeof(float), data, GL_STATIC_DRAW);
	glVertexAttribPointer(loc, size, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(loc);
}


/* Create a new mesh objct */
void backend_mesh_new(mesh_t* m, void *pos, void* nor, void *uv, void *tg, uint32_t *indices, uint32_t vertex_count, uint32_t index_count) {
	m->vertex_count = vertex_count;
	m->index_count = index_count;
	m->vbo_position = 0;
	m->vbo_normal = 0;
	m->vbo_uv = 0;

	glGenVertexArrays(1, &m->vao);
	glBindVertexArray(m->vao);

	// Vertex attribute buffers
	if (pos) push_buffer(&m->vbo_position, pos, vertex_count, 3, LOC_POSITION);
	if (nor) push_buffer(&m->vbo_normal, nor, vertex_count, 3, LOC_NORMAL);
	if (uv)  push_buffer(&m->vbo_uv, uv, vertex_count, 2, LOC_UV);
	if (tg)  push_buffer(&m->vbo_tangent, tg, vertex_count, 3, LOC_TANGENT);

	// Index buffer
	glGenBuffers(1, &m->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), indices, GL_STATIC_DRAW);

	// Instance model matrix buffer (locations 6-9)
	glGenBuffers(1, &m->instance_vbo_transform);
	glBindBuffer(GL_ARRAY_BUFFER, m->instance_vbo_transform);
	uint32_t loc = LOC_MODEL;
	for (uint32_t i = 0; i < 4; i++) {
		glVertexAttribPointer(loc + i, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (void *)(i * sizeof(vec4)));
		glEnableVertexAttribArray(loc + i);
		glVertexAttribDivisor(loc + i, 1);
	}

    // Instance uv_rect (location 10)
	glGenBuffers(1, &m->instance_vbo_uv_rect);
	glBindBuffer(GL_ARRAY_BUFFER, m->instance_vbo_uv_rect);
	glVertexAttribPointer(LOC_UV_RECT, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void *)0);
	glEnableVertexAttribArray(LOC_UV_RECT);
	glVertexAttribDivisor(LOC_UV_RECT, 1);

    glBindVertexArray(0);
}

void backend_mesh_destroy(mesh_t* m) {
	glDeleteVertexArrays(1, &m->vao);

	glDeleteBuffers(1, &m->instance_vbo_uv_rect);
	glDeleteBuffers(1, &m->instance_vbo_transform);
	glDeleteBuffers(1, &m->vbo_position);
	glDeleteBuffers(1, &m->vbo_normal);
	glDeleteBuffers(1, &m->vbo_uv);
	glDeleteBuffers(1, &m->vbo_tangent);
	glDeleteBuffers(1, &m->ebo);
}


/*******************************************************************************
 * BACKEND PLATFORM HANDLING
 *******************************************************************************/

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
			;
	}
}


void backend_texture_new(texture_t*  t, unsigned char* raw_pixel) {
	// Generate the ID
	glGenTextures(1, &t->id);

	// Bind to it
	glBindTexture(GL_TEXTURE_2D, t->id);

	// Set filtering parameter
	 switch (t->min_filter) {
		 case NEAREST:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			 break;
		 case LINEAR:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,  GL_LINEAR_MIPMAP_LINEAR);
			 break;
		default:
			 ;
	 }
	 switch (t->mag_filter) {
		 case NEAREST:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			 break;
		 case LINEAR:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			 break;
		default:
			 ;
	 }

	 // Set wrapping
	 switch (t->s_wrap) {
		 case REPEAT:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			 break;
		 case CLAMP:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			 break;
		default:
			 ;
	 }
	 switch (t->t_wrap) {
		 case REPEAT:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			 break;
		 case CLAMP:
			 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			 break;
		default:
			 ;
	 }

	 // Upload pixel data
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
		default:
			;
	 }

	 // Mipmap set
	 glGenerateMipmap(GL_TEXTURE_2D);

	 // Unbind
	glBindTexture(GL_TEXTURE_2D, 0);
}

void backend_texture_destroy(texture_t* t) {
	glDeleteTextures(1, &t->id);
}

void backend_ambient_factor_set(float f) {
	ambient_factor = f;
}

float backend_ambient_factor_get(void) {
	return ambient_factor;
}

void backend_shadow_map_new(shadow_map_t* sm) {
	// Step 1: create texture
	glGenTextures(1, &sm->id);
	glBindTexture(GL_TEXTURE_2D, sm->id);

	// Step 2: fill texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, sm->size, sm->size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	// Step 3: setup texture's filters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

	// Step 4: create FBO
	glGenFramebuffers(1, &sm->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, sm->fbo);

	// Step 5: attach the texture to the framebuffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sm->id, 0);

	// Step 6: tell OpenGL there is no color only depth
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	// Step 7: unbind
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void backend_shadow_map_destroy(shadow_map_t *sm) {
	glDeleteTextures(1, &sm->id);
	glDeleteFramebuffers(1, &sm->fbo);
}

void backend_set_camera_position(vec3 pos) {
	camera_position = pos;
}

void backend_set_active_frustum_corners(vec3* corners) {
    memcpy((void *)active_frustum_corners, (const void*)corners, 8 * sizeof(vec3));
}

