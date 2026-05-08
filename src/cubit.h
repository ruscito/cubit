// cubit.h
//
// Public API for game development. This is the only header games include.

#ifndef ENGINE_API_H
#define ENGINE_API_H

#include "cubit_types.h"

// Application
void application_quit(void);
double application_fps(void);
void fill_screen(color_t color);
void register_viewport_resized_callback(viewport_resized_func callback);

// Mesh
mesh_t* mesh_create(void *pos, void* nor, void *uv, void *tg, uint32_t *indices, uint32_t vertex_count, uint32_t index_count);
void mesh_destroy(mesh_t *m);
void mesh_draw(object3d_t *obj, mat4 pv);
void mesh_draw_instanced(mesh_t* mesh, mat4* transforms, uint32_t count, mat4 vp);

// helper
float lerp_precise(float a, float b, float t);
float lerp(float a, float b, float c);

// ---- 3D Object (Stage 23 — pool, full setter/getter set) -----------------
//
// Pattern mirrors object2d:
//   - object3d_new() returns a stable pointer from the engine's pool
//     (MAX_OBJECTS3D = 10000). object3d_destroy() returns the slot to the pool.
//   - object3d_set_active(false) disables an object without freeing the slot
//     — useful for object pooling (projectiles, particles, enemies).
//   - The game never calls a "submit" function. Active objects with a mesh
//     and material are collected automatically every frame and routed to the
//     opaque or transparent pass based on material->opacity.
//
// Color: per-instance tint, multiplied with material->surface_color CPU-side.
// Default is COLOR_WHITE so untinted objects show the material's color
// unchanged.
//
// Rotation: a vec3 of euler angles in radians (X then Y then Z applied).
// rotate_x/y/z are convenience accumulators; set_rotation overwrites.

object3d_t* object3d_new(void);
void object3d_destroy(object3d_t* obj);

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

// Material 3D
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

// Material 2D
material2d_t* material2d_create(void);
void material2d_destroy(material2d_t* m);
material2d_t* material2d_get_default(void);
void material2d_set_color(material2d_t* m, color_t c);
void material2d_set_diffuse_texture(material2d_t* m, texture_t* t);
void material2d_set_shader(material2d_t* m, shader_t* s);
void material2d_set_blend_mode(material2d_t* m, blend_mode_t mode);

// Camera 3D
camera_t* camera_target_new(int32_t projection_mode, vec3 position, vec3  up,
							float fov, float near, float far, float aspect,
							vec3 target);
camera_t* camera_free_new(int32_t projection_mode, vec3 position, vec3  up,
						  float fov, float near, float far, float aspect,
						  float yaw, float pitch);
void camera_destroy(camera_t* c);
void camera_move_forward(camera_t* c, float distance);
void camera_move_right(camera_t* c, float distance);
void camera_move_up(camera_t* c, float distance);
void camera_rotate_yaw(camera_t* c, float angle);
void camera_rotate_pitch(camera_t* c, float angle);
void camera_set_target(camera_t* c, vec3 target);
void camera_set_orbit(camera_t* c, float yaw_delta, float pitch_delta);
void camera_zoom(camera_t* c, float factor);
void camera_smooth_zoom(camera_t* c, float target_factor, float speed, double dt);
void camera_update_viewport(camera_t* c, int width, int height);
void camera_set_active(camera_t* c);
camera_t* camera_get_active(void);

// Camera 2D
camera2d_t* camera2d_new(vec2 position, float zoom, vec2 viewport_size, int32_t priority);
void camera2d_destroy(camera2d_t* cam);
void camera2d_set_position(camera2d_t* cam, vec2 position);
void camera2d_set_zoom(camera2d_t* cam, float zoom);
void camera2d_set_rotation(camera2d_t* cam, float radians);
void camera2d_set_priority(camera2d_t* cam, int32_t priority);
void camera2d_set_viewport(camera2d_t* cam, vec2 viewport_size);
vec2 camera2d_get_position(camera2d_t* cam);
float camera2d_get_zoom(camera2d_t* cam);
float camera2d_get_rotation(camera2d_t* cam);
int32_t camera2d_get_priority(camera2d_t* cam);
vec2 camera2d_get_viewport(camera2d_t* cam);
camera2d_t* camera2d_get_ui(void);
vec2 camera2d_screen_to_virtual(camera2d_t* cam, float screen_x, float screen_y,
                                 float window_w, float window_h);

// Object 2D
object2d_t* object2d_new(void);
void object2d_destroy(object2d_t* obj);
void object2d_set_position(object2d_t* obj, vec2 position);
void object2d_set_size(object2d_t* obj, vec2 size);
void object2d_set_rotation(object2d_t* obj, float radians);
void object2d_set_color(object2d_t* obj, color_t c);
void object2d_set_uv_rect(object2d_t* obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void object2d_set_flip(object2d_t* obj, bool flip_x, bool flip_y);
void object2d_set_material(object2d_t* obj, material2d_t* material);
void object2d_set_camera(object2d_t* obj, camera2d_t* camera);
void object2d_set_layer(object2d_t* obj, int32_t layer);
void object2d_set_active(object2d_t* obj, bool active);
void object2d_move(object2d_t* obj, vec2 delta);
vec2 object2d_get_position(object2d_t* obj);
vec2 object2d_get_size(object2d_t* obj);
float object2d_get_rotation(object2d_t* obj);
color_t object2d_get_color(object2d_t* obj);
vec4 object2d_get_uv_rect(object2d_t* obj);
material2d_t* object2d_get_material(object2d_t* obj);
camera2d_t* object2d_get_camera(object2d_t* obj);
int32_t object2d_get_layer(object2d_t* obj);
bool object2d_get_active(object2d_t* obj);

// Sprite (immediate-mode 2D)
void sprite_submit(const sprite_submit_t* cfg);

// UI Primitives
void ui_rect_filled(camera2d_t* cam,
                    float x, float y, float w, float h,
                    color_t color, int32_t layer);

void ui_rect_bordered(camera2d_t* cam,
                      float x, float y, float w, float h,
                      color_t fill_color, color_t border_color,
                      float border_width, int32_t layer);

void ui_progress_bar(camera2d_t* cam,
                     float x, float y, float w, float h,
                     float fill_pct,
                     color_t bg_color, color_t fill_color,
                     int32_t layer);

bool ui_point_in_rect(camera2d_t* cam,
                      float x, float y, float w, float h,
                      float point_x_screen, float point_y_screen);

bool ui_rect_contains_mouse(camera2d_t* cam,
                            float x, float y, float w, float h);

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
texture_t* texture_create_from_memory(unsigned char* pixels, uint32_t w, uint32_t h, uint32_t c, TextureTypes typ);
void texture_destroy(texture_t* t);

// Scene
void ambient_factor_set(float f);
float ambient_factor_get(void);

// Shadows
void shadow_distance_set(float sd);
float shadow_distance_get(void);
void shadow_set_cascade_lambda(float l);

// Collision
void collision_3d_resolve_slide(object3d_t* a, object3d_t* b);
void collision_3d_add_collidable(object3d_t* a);
void collision_3d_remove_collidable(object3d_t* a);
object3d_t* collision_3d_test_all(object3d_t* a, uint32_t* iter);
bool collision_3d_test(object3d_t* a, object3d_t* b);
raycast_3d_result_t collision_3d_raycast(vec3 origin, vec3 direction, float max_distance);

// Text
void text_draw(const char* text, uint32_t length, float  size, uint32_t x, uint32_t y, color_t color);

#endif
