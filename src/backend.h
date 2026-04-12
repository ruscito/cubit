// backend.h

#ifndef BACKEND_H
#define BACKEND_H

#include "cubit_types.h"

typedef struct { color_t color; uint8_t flags; } cmd_clear_data;
typedef struct { viewport_t viewport; } cmd_viewport_data;
typedef struct {
	mat4 transform;
	uint32_t vao;
	uint32_t vertex_count;
} cmd_draw_data;
typedef struct {
	mat4 vp;
	mat4 *m;
	uint32_t vao;
	uint32_t vbo;
	uint32_t vertex_count;
	uint32_t instance_count;
} cmd_draw_data_instanced;


typedef struct { uint32_t shader_id; uint32_t material_id; } cmd_state_data;

typedef struct {
	uint32_t type;
	uint32_t layer;
	uint32_t sort_key;
	union {
		cmd_clear_data clear;
		cmd_viewport_data viewport;
		cmd_draw_data draw;
		cmd_draw_data_instanced instanced_draw;
		cmd_state_data state;
	};
} render_command_t;


// Foward declaration graphic context
typedef struct gfx_context gfx_context;

//
// Graphic context functions
gfx_context *gfx_context_init(app_config_t* cfg);
void gfx_context_shutdown(gfx_context* ctx);

// Renderer specific functions
void renderer_begin_frame(void);
void renderer_end_frame(void);
void renderer_loop_setup(void);
double renderer_dt(void);
int renderer_should_close(void);
void renderer_init(gfx_context* ctx);
void renderer_shutdown(void);
void renderer_draw(void);
void renderer_submit(render_command_t *cmd);
void renderer_process_input(void);
void renderer_fixed_update(void);


// Backend general function for the frontend
void backend_mesh_new(mesh_t* m, void *pos, void* nor, void *uv, void *tg, uint32_t *indices, uint32_t vertex_count, uint32_t index_count);
void backend_mesh_destroy(mesh_t* m);
void backend_texture_new(texture_t*  t, unsigned char* raw_pixel);
void backend_texture_destroy(texture_t* t);
void backend_shutdown(void);
void backend_set_mouse_mode(int32_t mode);
void backend_ambient_factor_set(float f);
float backend_ambient_factor_get(void);
void backend_shadow_atlas_new(shadow_atlas_t* sa);
void backend_shadow_atlas_destroy(shadow_atlas_t* sa);
void backend_set_camera_position(vec3 pos);
void backend_set_active_frustum_corners(vec3* corners);
void backend_set_cascade_corners(uint32_t cascade, vec3* corners);
void backend_set_view_matrix(mat4 v);

#endif
