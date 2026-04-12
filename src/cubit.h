// cubit.h
//
// Public API for the game development
// This is the only header games need to include

#ifndef ENGINE_API_H
#define ENGINE_API_H

#include "cubit_types.h"

// Application
void application_quit(void);
void fill_screen(color_t color);
void set_camera(vec3 pos, vec3 targt, vec3 up, float fov, uint32_t mode, float near, float far, float aspect);
void register_viewport_resized_callback(viewport_resized_func callback);

// Mesh
mesh_t* mesh_create(void *pos, void* nor, void *uv, void *tg, uint32_t *indices, uint32_t vertex_count, uint32_t index_count);
void mesh_destroy(mesh_t *m);
void mesh_draw(object3d_t *obj, mat4 pv);
void mesh_draw_instanced(mesh_t* mesh, mat4* transforms, uint32_t count, mat4 vp);

// batching
void submit_object3d(object3d_t* obj);

// helper
float lerp_precise(float a, float b, float t);
float lerp(float a, float b, float c);

// Game Object related d functions
object3d_t* object3d_new(void);
void object3d_set_mesh(object3d_t *obj, mesh_t* mesh);
void object3d_set_material(object3d_t *obj, material_t* material);
void object3d_destroy(object3d_t *obj);
void object3d_rotate_x(object3d_t *obj, float angle);
void object3d_rotate_y(object3d_t *obj, float angle);
void object3d_rotate_z(object3d_t *obj, float angle);
vec3 object3d_get_position(object3d_t *obj);
void object3d_set_position(object3d_t *obj, vec3 pos);
mesh_t* object3d_get_mesh(object3d_t *obj);
material_t* object3d_get_material(object3d_t *obj);
mat4 object3d_get_transform(object3d_t *obj);
aabb_t object3d_get_aabb(object3d_t *obj);
void object3d_set_uv_rect(object3d_t* obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

// Input
int32_t is_key_pressed(InputKeyboardKey key);
int32_t is_key_released(InputKeyboardKey key);
int32_t is_key_down(InputKeyboardKey key);
void register_key_callback(process_keyboard_func callback);
int32_t is_mouse_button_pressed(InputMouseButton button);
void get_mouse_position(double* x, double* y);
void get_mouse_scroll(double* x_offset, double* y_offset);
int32_t is_mouse_inside(void);
void set_mouse_mode(int32_t mode);
void register_mouse_position_callback(process_mouse_position_func callback);
void register_mouse_button_callback(process_mouse_button_func callback);
void register_mouse_scroll_callback(process_mouse_scroll_func callback);
void register_mouse_enter_callback(process_mouse_enter_func callback);

// Shader
shader_t *shader_create(const char* vs, const char* fs);
int32_t shader_uniform_lookup(shader_t* s, char* uniform_name);
void shader_destroy(shader_t* s);

// Material
material_t* material_create(void);
void material_destroy(material_t* m);
void material_set_surface_color(material_t* m, color_t c);
void material_set_specular_color(material_t* m, color_t c);
void material_set_shininess(material_t* m, float value);
void material_set_roughness(material_t* m, float value);
void material_set_emissive_color(material_t* m, color_t c);
void material_set_diffuse_texture(material_t* m, texture_t* t);
void material_set_normal_texture(material_t* m, texture_t* t);
void material_set_opacity(material_t* m, float o);
void material_set_cast_shadow(material_t* m, bool v);

// Camera
camera_t* camera_target_new(int32_t projection_mode, vec3 position, vec3  up,
							float fov, float near, float far, float aspect,
							vec3 target);
camera_t* camera_free_new(int32_t projection_mode, vec3 position, vec3  up,
						  float fov, float near, float far, float aspect,
						  float yaw, float pitch);
void camera_destroy(camera_t* c);
void camera_move_forward(camera_t* c, float distance);		// both mode
void camera_move_right(camera_t* c, float distance);		// both mode
void camera_move_up(camera_t* c, float distance);		// both mode
void camera_rotate_yaw(camera_t* c, float angle);
void camera_rotate_pitch(camera_t* c, float angle);
void camera_set_target(camera_t* c, vec3 target);
void camera_set_orbit(camera_t* c, float yaw_delta, float pitch_delta);
void camera_zoom(camera_t* c, float factor);
void camera_smooth_zoom(camera_t* c, float target_factor, float speed, double dt);
void camera_update_viewport(camera_t* c, int width, int height); // Internally calculates and sets aspect ratio
void camera_set_active(camera_t* c);
camera_t* camera_get_active(void);

// Light
int32_t light_create(LightTypes type);
void light_destroy(int32_t index);
void light_set_color(int32_t index, color_t color);
void light_set_intensity(int32_t index, float intensity);
void light_set_direction(int32_t index, vec3 direction);
void light_set_position(int32_t index, vec3 position);
void light_set_attenuation(int32_t index, float constant, float linear, float quadratic);
void light_set_cone(int32_t index, float inner_degrees, float outer_degrees);
void light_enable_shadow(int32_t index);
void light_disable_shadow(int32_t index);

// Texture
texture_t* texture_create(const char* filename, TextureTypes typ);
void texture_destroy(texture_t* t);

// Scene related
void ambient_factor_set(float f);
float ambient_factor_get(void);

// Shadow map
void shadow_distance_set(float sd);
float shadow_distance_get(void);
void shadow_set_cascade_lambda(float l);

#endif
