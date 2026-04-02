// game.c — Lighting test scene

#include "cubit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define CAMERA_SPEED 3.0f
#define ROTATION_SPEED 1.0f

static camera_t* camera;
static float zoom = 1.0f;
// Texture
static texture_t* cube_texture;
static texture_t* cube_normal_texture;

// Meshes
static mesh_t* cube_mesh;
static mesh_t* floor_mesh;

// Materials
static material_t* mat_white;
static material_t* mat_red;
static material_t* mat_blue;
static material_t* mat_green;
static material_t* mat_yellow;

// Objects
static object3d_t* floor_obj;
static object3d_t* center_cube;
static object3d_t* left_cube;
static object3d_t* right_cube;
static object3d_t* back_cube;
static object3d_t* front_cube;

// Lights
static int32_t sun;
static int32_t point_light;
static int32_t spot_light;

// Shadow map
static shadow_map_t* shadow_map;
static shadow_map_t* shadow_map_spot;

// 24 unique vertices (4 per face)
static float cube_positions[] = {
	// Front face
	-0.5f, -0.5f,  0.5f,
	 0.5f, -0.5f,  0.5f,
	 0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f,  0.5f,
	// Back face
	 0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f,
	-0.5f,  0.5f, -0.5f,
	 0.5f,  0.5f, -0.5f,
	// Left face
	-0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f,  0.5f,
	-0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f, -0.5f,
	// Right face
	 0.5f, -0.5f,  0.5f,
	 0.5f, -0.5f, -0.5f,
	 0.5f,  0.5f, -0.5f,
	 0.5f,  0.5f,  0.5f,
	// Top face
	-0.5f,  0.5f,  0.5f,
	 0.5f,  0.5f,  0.5f,
	 0.5f,  0.5f, -0.5f,
	-0.5f,  0.5f, -0.5f,
	// Bottom face
	-0.5f, -0.5f, -0.5f,
	 0.5f, -0.5f, -0.5f,
	 0.5f, -0.5f,  0.5f,
	-0.5f, -0.5f,  0.5f,
};

static float cube_normals[] = {
	// Front face
	 0.0f,  0.0f,  1.0f,
	 0.0f,  0.0f,  1.0f,
	 0.0f,  0.0f,  1.0f,
	 0.0f,  0.0f,  1.0f,
	// Back face
	 0.0f,  0.0f, -1.0f,
	 0.0f,  0.0f, -1.0f,
	 0.0f,  0.0f, -1.0f,
	 0.0f,  0.0f, -1.0f,
	// Left face
	-1.0f,  0.0f,  0.0f,
	-1.0f,  0.0f,  0.0f,
	-1.0f,  0.0f,  0.0f,
	-1.0f,  0.0f,  0.0f,
	// Right face
	 1.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,
	// Top face
	 0.0f,  1.0f,  0.0f,
	 0.0f,  1.0f,  0.0f,
	 0.0f,  1.0f,  0.0f,
	 0.0f,  1.0f,  0.0f,
	// Bottom face
	 0.0f, -1.0f,  0.0f,
	 0.0f, -1.0f,  0.0f,
	 0.0f, -1.0f,  0.0f,
	 0.0f, -1.0f,  0.0f,
};

static float cube_uvs[] = {
    // Front face
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // Back face
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // Left face
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // Right face
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // Top face
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
    // Bottom face
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
};

static float cube_tangents[] = {
    // Front face
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
    // Back face
    -1.0f,  0.0f,  0.0f,
    -1.0f,  0.0f,  0.0f,
    -1.0f,  0.0f,  0.0f,
    -1.0f,  0.0f,  0.0f,
    // Left face
     0.0f,  0.0f,  1.0f,
     0.0f,  0.0f,  1.0f,
     0.0f,  0.0f,  1.0f,
     0.0f,  0.0f,  1.0f,
    // Right face
     0.0f,  0.0f, -1.0f,
     0.0f,  0.0f, -1.0f,
     0.0f,  0.0f, -1.0f,
     0.0f,  0.0f, -1.0f,
    // Top face
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
    // Bottom face
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
};



static uint32_t cube_indices[] = {
	 0,  1,  2,   2,  3,  0,
	 4,  5,  6,   6,  7,  4,
	 8,  9, 10,  10, 11,  8,
	12, 13, 14,  14, 15, 12,
	16, 17, 18,  18, 19, 16,
	20, 21, 22,  22, 23, 20,
};

// Floor — a wide flat box
static float floor_positions[] = {
	// Top face (the visible one)
	-10.0f, 0.0f,  10.0f,
	 10.0f, 0.0f,  10.0f,
	 10.0f, 0.0f, -10.0f,
	-10.0f, 0.0f, -10.0f,
	// Bottom face
	-10.0f, -0.2f, -10.0f,
	 10.0f, -0.2f, -10.0f,
	 10.0f, -0.2f,  10.0f,
	-10.0f, -0.2f,  10.0f,
	// Front face
	-10.0f, -0.2f,  10.0f,
	 10.0f, -0.2f,  10.0f,
	 10.0f,  0.0f,  10.0f,
	-10.0f,  0.0f,  10.0f,
	// Back face
	 10.0f, -0.2f, -10.0f,
	-10.0f, -0.2f, -10.0f,
	-10.0f,  0.0f, -10.0f,
	 10.0f,  0.0f, -10.0f,
	// Left face
	-10.0f, -0.2f, -10.0f,
	-10.0f, -0.2f,  10.0f,
	-10.0f,  0.0f,  10.0f,
	-10.0f,  0.0f, -10.0f,
	// Right face
	 10.0f, -0.2f,  10.0f,
	 10.0f, -0.2f, -10.0f,
	 10.0f,  0.0f, -10.0f,
	 10.0f,  0.0f,  10.0f,
};

static float floor_normals[] = {
	// Top face
	 0.0f,  1.0f,  0.0f,
	 0.0f,  1.0f,  0.0f,
	 0.0f,  1.0f,  0.0f,
	 0.0f,  1.0f,  0.0f,
	// Bottom face
	 0.0f, -1.0f,  0.0f,
	 0.0f, -1.0f,  0.0f,
	 0.0f, -1.0f,  0.0f,
	 0.0f, -1.0f,  0.0f,
	// Front face
	 0.0f,  0.0f,  1.0f,
	 0.0f,  0.0f,  1.0f,
	 0.0f,  0.0f,  1.0f,
	 0.0f,  0.0f,  1.0f,
	// Back face
	 0.0f,  0.0f, -1.0f,
	 0.0f,  0.0f, -1.0f,
	 0.0f,  0.0f, -1.0f,
	 0.0f,  0.0f, -1.0f,
	// Left face
	-1.0f,  0.0f,  0.0f,
	-1.0f,  0.0f,  0.0f,
	-1.0f,  0.0f,  0.0f,
	-1.0f,  0.0f,  0.0f,
	// Right face
	 1.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,
};

static float floor_uvs[] = {
    // Top face
    0.0f, 0.0f,
    10.0f, 0.0f,
    10.0f, 10.0f,
    0.0f, 10.0f,
    // Bottom face
    0.0f, 0.0f,
    10.0f, 0.0f,
    10.0f, 10.0f,
    0.0f, 10.0f,
    // Front face
    0.0f, 0.0f,
    10.0f, 0.0f,
    10.0f, 1.0f,
    0.0f, 1.0f,
    // Back face
    0.0f, 0.0f,
    10.0f, 0.0f,
    10.0f, 1.0f,
    0.0f, 1.0f,
    // Left face
    0.0f, 0.0f,
    10.0f, 0.0f,
    10.0f, 1.0f,
    0.0f, 1.0f,
    // Right face
    0.0f, 0.0f,
    10.0f, 0.0f,
    10.0f, 1.0f,
    0.0f, 1.0f,
};

static float floor_tangents[] = {
    // Top face
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
    // Bottom face
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
    // Front face
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
     1.0f,  0.0f,  0.0f,
    // Back face
    -1.0f,  0.0f,  0.0f,
    -1.0f,  0.0f,  0.0f,
    -1.0f,  0.0f,  0.0f,
    -1.0f,  0.0f,  0.0f,
    // Left face
     0.0f,  0.0f,  1.0f,
     0.0f,  0.0f,  1.0f,
     0.0f,  0.0f,  1.0f,
     0.0f,  0.0f,  1.0f,
    // Right face
     0.0f,  0.0f, -1.0f,
     0.0f,  0.0f, -1.0f,
     0.0f,  0.0f, -1.0f,
     0.0f,  0.0f, -1.0f,
};


static uint32_t floor_indices[] = {
	 0,  1,  2,   2,  3,  0,
	 4,  5,  6,   6,  7,  4,
	 8,  9, 10,  10, 11,  8,
	12, 13, 14,  14, 15, 12,
	16, 17, 18,  18, 19, 16,
	20, 21, 22,  22, 23, 20,
};


void process_keyboard(int32_t key, int32_t scancode, int32_t action, int32_t mods) {
	UNUSED_ARG(key); UNUSED_ARG(scancode); UNUSED_ARG(action); UNUSED_ARG(mods);
}

void process_mouse_position(double x_pos, double y_pos) {
	UNUSED_ARG(x_pos); UNUSED_ARG(y_pos);
}

void process_mouse_button(int32_t button, int32_t action, int32_t mods) {
	UNUSED_ARG(button); UNUSED_ARG(action); UNUSED_ARG(mods);
}

void process_mouse_scroll(double x_offset, double y_offset) {
	UNUSED_ARG(x_offset); UNUSED_ARG(y_offset);
	camera_rotate_pitch(camera, y_offset * DEG2RAD);
}

void process_mouse_enter(int32_t entered) {
	UNUSED_ARG(entered);
}

void screen_resized(int32_t width, int32_t height) {
	camera_update_viewport(camera, width, height);
}

void application_config(app_config_t* cfg) {
	cfg->width = 1600;
	cfg->height = 900;
	cfg->title = "Cubit Lighting Test";
	cfg->fps = 60;

	camera = camera_free_new(
		PERSPECTIVE,
		(vec3){5.0f, 4.0f, 8.0f},
		(vec3){0.0f, 1.0f, 0.0f},
		45.0f * DEG2RAD,
		0.01f,
		1000.0f,
		(float)cfg->width / (float)cfg->height,
		-120.0f * DEG2RAD, -20.0f * DEG2RAD);

	register_key_callback(process_keyboard);
	register_mouse_position_callback(process_mouse_position);
	register_mouse_button_callback(process_mouse_button);
	register_mouse_scroll_callback(process_mouse_scroll);
	register_mouse_enter_callback(process_mouse_enter);
	register_viewport_resized_callback(screen_resized);
}

void application_init(void) {
	// Texture
	cube_texture = texture_create("./bin/assets/packed_cube.png", COLOR_TEXTURE);
	cube_normal_texture = texture_create("./bin/assets/cube_normal.png", DATA_TEXTURE);
	if (!cube_texture) exit(-1);
	if (!cube_normal_texture) exit(-1);

	// Meshes
	cube_mesh = mesh_create(cube_positions, cube_normals, cube_uvs, cube_tangents, cube_indices, 24, 36);
	floor_mesh = mesh_create(floor_positions, floor_normals, floor_uvs, floor_tangents , floor_indices, 24, 36);

	// Materials
	mat_white = material_create();
	material_set_surface_color(mat_white, CUBIT_WHITE);
	material_set_shininess(mat_white, 64.0f);
	material_set_normal_texture(mat_white, cube_normal_texture);

	mat_red = material_create();
	material_set_surface_color(mat_red, CUBIT_RED);
	material_set_specular_color(mat_red, COLOR_WHITE);
	material_set_shininess(mat_red, 32.0f);
    material_set_opacity(mat_red, 0.3f);
    material_set_cast_shadow(mat_red, false);


	mat_blue = material_create();
	material_set_surface_color(mat_blue, CUBIT_BLUE);
	material_set_specular_color(mat_blue, COLOR_WHITE);
	material_set_shininess(mat_blue, 128.0f);
	material_set_diffuse_texture(mat_blue, cube_texture);

	mat_green = material_create();
	material_set_surface_color(mat_green, CUBIT_GREEN);
	material_set_specular_color(mat_green, COLOR_WHITE);
	material_set_shininess(mat_green, 16.0f);
	material_set_normal_texture(mat_green, cube_normal_texture);

	mat_yellow = material_create();
	material_set_surface_color(mat_yellow, CUBIT_YELLOW);
	material_set_specular_color(mat_yellow, COLOR_WHITE);
	material_set_shininess(mat_yellow, 256.0f);

	// Floor
	floor_obj = object3d_new();
	object3d_set_mesh(floor_obj, floor_mesh);
	object3d_set_material(floor_obj, mat_white);
	object3d_set_position(floor_obj, (vec3){0.0f, -0.5f, 0.0f});

	// Cubes arranged in a cross pattern
	center_cube = object3d_new();
	object3d_set_mesh(center_cube, cube_mesh);
	object3d_set_material(center_cube, mat_red);
	object3d_set_position(center_cube, (vec3){0.0f, 0.0f, 0.0f});

	left_cube = object3d_new();
	object3d_set_mesh(left_cube, cube_mesh);
	object3d_set_material(left_cube, mat_blue);
	object3d_set_position(left_cube, (vec3){-3.0f, 0.0f, 0.0f});
    object3d_set_uv_rect(left_cube, 2, 219, 32, 32);

	right_cube = object3d_new();
	object3d_set_mesh(right_cube, cube_mesh);
	object3d_set_material(right_cube, mat_green);
	object3d_set_position(right_cube, (vec3){3.0f, 0.0f, 0.0f});

	back_cube = object3d_new();
	object3d_set_mesh(back_cube, cube_mesh);
	object3d_set_material(back_cube, mat_blue);
	object3d_set_position(back_cube, (vec3){0.0f, 0.0f, -3.0f});
    object3d_set_uv_rect(back_cube, 2, 115, 64, 64);

	front_cube = object3d_new();
	object3d_set_mesh(front_cube, cube_mesh);
	object3d_set_material(front_cube, mat_blue);
	object3d_set_position(front_cube, (vec3){0.0f, 0.0f, 3.0f});
    object3d_set_uv_rect(front_cube, 2, 2, 89, 109);

	// Directional light — warm sun from upper-left
	sun = light_create(LIGHT_DIRECTIONAL);
	light_set_direction(sun, (vec3){-1.0f, -1.0f, -0.5f});
	light_set_color(sun, (color_t){1.0f, 0.95f, 0.85f, 1.0f});
	light_set_intensity(sun, 0.8f);
	shadow_map = shadow_map_create();
	light_set_shadow_map(sun, shadow_map);

	// Point light — cool blue hovering above center
	point_light = light_create(LIGHT_POINT);
	light_set_position(point_light, (vec3){0.0f, 3.0f, 0.0f});
	light_set_color(point_light, (color_t){0.4f, 0.6f, 1.0f, 1.0f});
	light_set_intensity(point_light, 1.2f);

    // Spot light -- warm red overing abocve and to the side of the green cube
	spot_light = light_create(LIGHT_SPOT);
	light_set_position(spot_light, (vec3){3.0f,4.0f, 2.0f});
	light_set_direction(spot_light, (vec3){0.0f, -1.0f, -0.5f});
	light_set_color(spot_light, CUBIT_RED);
	light_set_intensity(spot_light, 0.5);
    shadow_map_spot = shadow_map_create();
    light_set_shadow_map(spot_light, shadow_map_spot);
}

void application_fixed_update(double dt) {
    UNUSED_ARG(dt);
}

void application_update(double dt) {
	fill_screen((color_t){0.08f, 0.08f, 0.12f, 1.0f});

	// Slow rotation on cubes
	object3d_rotate_y(center_cube, 30.0f * dt);
	object3d_rotate_y(left_cube, -20.0f * dt);
	object3d_rotate_y(right_cube, 15.0f * dt);
	object3d_rotate_y(back_cube, -25.0f * dt);
	object3d_rotate_y(front_cube, 20.0f * dt);

	// Camera controls
	if (is_key_released(KEY_ESC)) application_quit();

	if (is_key_down(KEY_W)) camera_move_forward(camera, CAMERA_SPEED * dt);
	if (is_key_down(KEY_S)) camera_move_forward(camera, -CAMERA_SPEED * dt);
	if (is_key_down(KEY_D)) camera_move_right(camera, CAMERA_SPEED * dt);
	if (is_key_down(KEY_A)) camera_move_right(camera, -CAMERA_SPEED * dt);
	if (is_key_down(KEY_E)) camera_move_up(camera, CAMERA_SPEED * dt);
	if (is_key_down(KEY_Q)) camera_move_up(camera, -CAMERA_SPEED * dt);

	if (is_key_down(KEY_UP)) camera_rotate_pitch(camera, ROTATION_SPEED * DEG2RAD);
	if (is_key_down(KEY_DOWN)) camera_rotate_pitch(camera, -ROTATION_SPEED * DEG2RAD);
	if (is_key_down(KEY_LEFT)) camera_rotate_yaw(camera, -ROTATION_SPEED * DEG2RAD);
	if (is_key_down(KEY_RIGHT)) camera_rotate_yaw(camera, ROTATION_SPEED * DEG2RAD);

	if (is_key_pressed(KEY_1)) zoom = 1.0f;
	if (is_key_pressed(KEY_2)) zoom = 2.0f;
	if (is_key_pressed(KEY_4)) zoom = 4.0f;

	camera_smooth_zoom(camera, zoom, 5.0f, dt);

	// Submit everything
    camera_set_active(camera);
	submit_object3d(floor_obj);
	submit_object3d(center_cube);
	submit_object3d(left_cube);
	submit_object3d(right_cube);
	submit_object3d(back_cube);
	submit_object3d(front_cube);
}

void application_shutdown(void) {
	object3d_destroy(floor_obj);
	object3d_destroy(center_cube);
	object3d_destroy(left_cube);
	object3d_destroy(right_cube);
	object3d_destroy(back_cube);
	object3d_destroy(front_cube);

	texture_destroy(cube_texture);
	texture_destroy(cube_normal_texture);

	shadow_map_destroy(shadow_map);
	shadow_map_destroy(shadow_map_spot);
	material_destroy(mat_white);
	material_destroy(mat_red);
	material_destroy(mat_blue);
	material_destroy(mat_green);
	material_destroy(mat_yellow);

	mesh_destroy(cube_mesh);
	mesh_destroy(floor_mesh);


	camera_destroy(camera);
}
