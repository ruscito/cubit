// game3d.c — Stage 23 demo scene
//
// Demonstrates the post-Stage-23 3D API:
//   - object3d_new() returns a pooled, stable pointer.
//   - No submit_object3d() — the engine collects active objects automatically.
//   - object3d_set_color() per-instance tint, multiplied with material's
//     surface_color CPU-side. The 100 cube grid below shows off the case
//     where many instances share a single mesh+material but render with
//     different colors in a single draw call.
//   - object3d_set_active(false) disables an object without freeing it.
//
// Color pipeline (Stage 23): a CUBIT_GREEN cube renders as the same on-screen
// green as a `ui_rect_filled(..., CUBIT_GREEN, ...)` call.

#include "cubit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define CAMERA_SPEED   3.0f
#define ROTATION_SPEED 1.0f


static camera_t* camera;
static float zoom = 1.0f;

// Textures
static texture_t* cube_texture;
static texture_t* cube_normal_texture;

// Meshes
static mesh_t* cube_mesh;
static mesh_t* floor_mesh;

// Materials
static material_t* mat_default;     // surface_color = white, used for the per-instance grid
static material_t* mat_floor;
static material_t* mat_glass;       // transparent — cast_shadow off

// Hand-picked objects
static object3d_t* floor_obj;
static object3d_t* player_cube;
static object3d_t* glass_cube;

// Per-instance color demo: 10x10 grid sharing one mesh + one material.
// All 100 cubes render in a single glDrawElementsInstanced call thanks to
// the batch's mesh+material grouping.
#define GRID_W 10
#define GRID_H 10
static object3d_t* grid_cubes[GRID_W * GRID_H];

// Lights
static int32_t sun;
static int32_t point_light;


// ---- Cube and floor mesh data --------------------------------------------

static float cube_positions[] = {
	-0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
	 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
	 0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
};

static float cube_normals[] = {
	 0, 0, 1,  0, 0, 1,  0, 0, 1,  0, 0, 1,
	 0, 0,-1,  0, 0,-1,  0, 0,-1,  0, 0,-1,
	-1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0,
	 1, 0, 0,  1, 0, 0,  1, 0, 0,  1, 0, 0,
	 0, 1, 0,  0, 1, 0,  0, 1, 0,  0, 1, 0,
	 0,-1, 0,  0,-1, 0,  0,-1, 0,  0,-1, 0,
};

static float cube_uvs[] = {
	0,0, 1,0, 1,1, 0,1,
	0,0, 1,0, 1,1, 0,1,
	0,0, 1,0, 1,1, 0,1,
	0,0, 1,0, 1,1, 0,1,
	0,0, 1,0, 1,1, 0,1,
	0,0, 1,0, 1,1, 0,1,
};

static float cube_tangents[] = {
	 1,0,0,  1,0,0,  1,0,0,  1,0,0,
	-1,0,0, -1,0,0, -1,0,0, -1,0,0,
	 0,0,1,  0,0,1,  0,0,1,  0,0,1,
	 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1,
	 1,0,0,  1,0,0,  1,0,0,  1,0,0,
	 1,0,0,  1,0,0,  1,0,0,  1,0,0,
};

static uint32_t cube_indices[] = {
	 0, 1, 2, 2, 3, 0,
	 4, 5, 6, 6, 7, 4,
	 8, 9,10,10,11, 8,
	12,13,14,14,15,12,
	16,17,18,18,19,16,
	20,21,22,22,23,20,
};


static float floor_positions[] = {
	-15.0f, 0.0f,  15.0f,  15.0f, 0.0f,  15.0f,  15.0f, 0.0f, -15.0f, -15.0f, 0.0f, -15.0f,
};
static float floor_normals[]   = { 0,1,0,  0,1,0,  0,1,0,  0,1,0 };
static float floor_uvs[]       = { 0,0,  20,0,  20,20,  0,20 };
static float floor_tangents[]  = { 1,0,0,  1,0,0,  1,0,0,  1,0,0 };
static uint32_t floor_indices[] = { 0,1,2, 2,3,0 };


// ---- Color palette for the grid ------------------------------------------
//
// Picked so the WYSIWYG sRGB pipeline has visible diversity: a saturated
// CUBIT_GREEN at (0.686, 0.843, 0.373) should land on screen as the same
// green as `ui_rect_filled(... CUBIT_GREEN ...)` rendered next to the grid.

static const color_t GRID_PALETTE[] = {
	CUBIT_RED, CUBIT_GREEN, CUBIT_BLUE, CUBIT_YELLOW, CUBIT_BROWN,
	CUBIT_WHITE, CUBIT_GRAY, CUBIT_BLACK,
	{0.95f, 0.40f, 0.70f, 1.0f}, // pink
	{0.40f, 0.95f, 0.95f, 1.0f}, // cyan
	{0.55f, 0.30f, 0.75f, 1.0f}, // violet
	{0.95f, 0.55f, 0.20f, 1.0f}, // orange
};
static const uint32_t PALETTE_SIZE = sizeof(GRID_PALETTE) / sizeof(GRID_PALETTE[0]);


// ---- Input callbacks (no-ops here, registered for completeness) ----------

void process_keyboard(int32_t k, int32_t s, int32_t a, int32_t m) {
	UNUSED_ARG(k); UNUSED_ARG(s); UNUSED_ARG(a); UNUSED_ARG(m);
}
void process_mouse_position(double x, double y) { UNUSED_ARG(x); UNUSED_ARG(y); }
void process_mouse_button(int32_t b, int32_t a, int32_t m) {
	UNUSED_ARG(b); UNUSED_ARG(a); UNUSED_ARG(m);
}
void process_mouse_scroll(double x_offset, double y_offset) {
	UNUSED_ARG(x_offset);
	camera_rotate_pitch(camera, y_offset * DEG2RAD);
}
void process_mouse_enter(int32_t entered) { UNUSED_ARG(entered); }

void screen_resized(int32_t width, int32_t height) {
	camera_update_viewport(camera, width, height);
}


void application_config(app_config_t* cfg) {
	cfg->width  = 1600;
	cfg->height = 900;
	cfg->title  = "Cubit — Stage 23 demo";
	cfg->fps    = 60;

	camera = camera_free_new(
		PERSPECTIVE,
		(vec3){8.0f, 6.0f, 12.0f},
		(vec3){0.0f, 1.0f, 0.0f},
		45.0f * DEG2RAD,
		0.01f,
		1000.0f,
		(float)cfg->width / (float)cfg->height,
		-120.0f * DEG2RAD, -25.0f * DEG2RAD);

	register_key_callback(process_keyboard);
	register_mouse_position_callback(process_mouse_position);
	register_mouse_button_callback(process_mouse_button);
	register_mouse_scroll_callback(process_mouse_scroll);
	register_mouse_enter_callback(process_mouse_enter);
	register_viewport_resized_callback(screen_resized);
}


void application_init(void) {
	cube_texture        = texture_create("./bin/assets/packed_cube.png", COLOR_TEXTURE);
	cube_normal_texture = texture_create("./bin/assets/cube_normal.png", DATA_TEXTURE);
	if (!cube_texture)        fprintf(stderr, "warning: packed_cube.png missing — using default texture\n");
	if (!cube_normal_texture) fprintf(stderr, "warning: cube_normal.png missing — using default normal\n");

	cube_mesh  = mesh_create(cube_positions, cube_normals, cube_uvs, cube_tangents,
	                          cube_indices, 24, 36);
	floor_mesh = mesh_create(floor_positions, floor_normals, floor_uvs, floor_tangents,
	                          floor_indices, 4, 6);

	// One material shared by everything in the grid. The per-instance colors
	// will multiply with this material's surface_color (white) so they pass
	// through unchanged — but having the multiplication path active keeps
	// the demo honest: changing the material color tints the whole grid.
	mat_default = material_create();
	material_set_surface_color(mat_default, COLOR_WHITE);
	material_set_shininess(mat_default, 64.0f);
	if (cube_normal_texture) material_set_normal_texture(mat_default, cube_normal_texture);

	mat_floor = material_create();
	material_set_surface_color(mat_floor, CUBIT_WHITE);
	material_set_shininess(mat_floor, 32.0f);

	mat_glass = material_create();
	material_set_surface_color(mat_glass, CUBIT_BLUE);
	material_set_specular_color(mat_glass, COLOR_WHITE);
	material_set_shininess(mat_glass, 128.0f);
	material_set_opacity(mat_glass, 0.4f);
	material_set_cast_shadow(mat_glass, false);

	// Floor
	floor_obj = object3d_new();
	object3d_set_mesh(floor_obj, floor_mesh);
	object3d_set_material(floor_obj, mat_floor);
	object3d_set_position(floor_obj, (vec3){0.0f, 0.0f, 0.0f});

	// Player cube — controllable, demonstrates per-instance color override.
	player_cube = object3d_new();
	object3d_set_mesh(player_cube, cube_mesh);
	object3d_set_material(player_cube, mat_default);
	object3d_set_color(player_cube, CUBIT_RED);
	object3d_set_position(player_cube, (vec3){0.0f, 0.5f, 5.0f});

	// Transparent glass cube
	glass_cube = object3d_new();
	object3d_set_mesh(glass_cube, cube_mesh);
	object3d_set_material(glass_cube, mat_glass);
	object3d_set_position(glass_cube, (vec3){-5.0f, 0.5f, 0.0f});

	// 10x10 grid sharing mesh + material. Each cube has a per-instance color
	// from the palette. They batch into ONE draw call.
	for (uint32_t y = 0; y < GRID_H; y++) {
		for (uint32_t x = 0; x < GRID_W; x++) {
			object3d_t* o = object3d_new();
			object3d_set_mesh(o, cube_mesh);
			object3d_set_material(o, mat_default);
			object3d_set_color(o, GRID_PALETTE[(x + y * 3) % PALETTE_SIZE]);
			object3d_set_position(o, (vec3){
				(float)x - GRID_W * 0.5f + 3.0f,
				0.5f,
				(float)y - GRID_H * 0.5f - 2.0f,
			});
			object3d_set_scale(o, (vec3){0.7f, 0.7f, 0.7f});
			grid_cubes[x + y * GRID_W] = o;
		}
	}

	// Sun
	sun = light_create(LIGHT_DIRECTIONAL);
	light_set_direction(sun, (vec3){-1.0f, -1.2f, -0.6f});
	light_set_color(sun, (color_t){1.0f, 0.95f, 0.85f, 1.0f});
	light_set_intensity(sun, 0.9f);
	light_enable_shadow(sun);

	// Point light
	point_light = light_create(LIGHT_POINT);
	light_set_position(point_light, (vec3){0.0f, 4.0f, 0.0f});
	light_set_color(point_light, (color_t){0.4f, 0.6f, 1.0f, 1.0f});
	light_set_intensity(point_light, 1.0f);

	// Collisions
	collision_3d_add_collidable(floor_obj);
	collision_3d_add_collidable(player_cube);
	collision_3d_add_collidable(glass_cube);
}


void application_fixed_update(double dt) { UNUSED_ARG(dt); }


void application_update(double dt) {
	fill_screen((color_t){0.08f, 0.08f, 0.12f, 1.0f});

	// Rotate the player cube continuously.
	object3d_rotate_y(player_cube, 30.0f * DEG2RAD * dt);

	// Camera controls
	if (is_key_released(KEY_ESC)) application_quit();
	if (is_key_down(KEY_W)) camera_move_forward(camera, CAMERA_SPEED * dt);
	if (is_key_down(KEY_S)) camera_move_forward(camera, -CAMERA_SPEED * dt);
	if (is_key_down(KEY_D)) camera_move_right(camera, CAMERA_SPEED * dt);
	if (is_key_down(KEY_A)) camera_move_right(camera, -CAMERA_SPEED * dt);
	if (is_key_down(KEY_E)) camera_move_up(camera, CAMERA_SPEED * dt);
	if (is_key_down(KEY_Q)) camera_move_up(camera, -CAMERA_SPEED * dt);

	if (is_key_down(KEY_UP))    camera_rotate_pitch(camera,  ROTATION_SPEED * DEG2RAD);
	if (is_key_down(KEY_DOWN))  camera_rotate_pitch(camera, -ROTATION_SPEED * DEG2RAD);
	if (is_key_down(KEY_LEFT))  camera_rotate_yaw(camera,   -ROTATION_SPEED * DEG2RAD);
	if (is_key_down(KEY_RIGHT)) camera_rotate_yaw(camera,    ROTATION_SPEED * DEG2RAD);

	// Toggle player cube on/off — demonstrates set_active without destroying.
	if (is_key_pressed(KEY_SPACE)) {
		object3d_set_active(player_cube, !object3d_get_active(player_cube));
	}

	if (is_key_pressed(KEY_1)) zoom = 1.0f;
	if (is_key_pressed(KEY_2)) zoom = 2.0f;
	if (is_key_pressed(KEY_4)) zoom = 4.0f;
	camera_smooth_zoom(camera, zoom, 5.0f, dt);

	// HUD: WYSIWYG color check — same value (CUBIT_GREEN) used on a 3D cube
	// in the grid will appear the same on-screen as the UI rect rendered
	// here. If they don't match, the sRGB pipeline (Stage 23 step 7) is
	// broken.
	camera2d_t* ui_cam = camera2d_get_ui();
	ui_rect_filled(ui_cam, 20, 20, 200, 50, CUBIT_GREEN, 0);
	ui_rect_bordered(ui_cam, 20, 80, 200, 50, CUBIT_RED, COLOR_WHITE, 3.0f, 0);
	ui_progress_bar(ui_cam, 20, 140, 200, 30, 0.5f, COLOR_GRAY, CUBIT_BLUE, 0);

	char fps_text[32];
	sprintf(fps_text, "FPS: %.0f", application_fps());
	text_draw(fps_text, strlen(fps_text), 1.5f, 20, 200, CUBIT_GREEN);

	const char* hint = "[Space] toggle cube  [WASD/QE] move  [Arrows] look";
	text_draw(hint, strlen(hint), 0.8f, 20, 240, CUBIT_WHITE);

	// Stage 23 single point of contact with the 3D pipeline:
	camera_set_active(camera);
	// — that's it. No submit_object3d() loop; the engine collects active
	// objects automatically in renderer_draw via object3d_collect_into_batch.
}


void application_shutdown(void) {
	for (uint32_t i = 0; i < GRID_W * GRID_H; i++) object3d_destroy(grid_cubes[i]);
	object3d_destroy(glass_cube);
	object3d_destroy(player_cube);
	object3d_destroy(floor_obj);

	if (cube_texture)        texture_destroy(cube_texture);
	if (cube_normal_texture) texture_destroy(cube_normal_texture);

	material_destroy(mat_default);
	material_destroy(mat_floor);
	material_destroy(mat_glass);

	mesh_destroy(cube_mesh);
	mesh_destroy(floor_mesh);

	camera_destroy(camera);
}
