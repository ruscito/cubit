// camera2d.c

#include "camera2d.h"

#include <stdio.h>


static struct {
	camera2d_t cameras[MAX_CAMERAS2D];
	uint32_t count;              // number of active cameras
	camera2d_t* ui_camera;       // pointer to built-in UI camera
} camera_table;


static void reset_camera(camera2d_t* cam) {
	cam->position = (vec2){0.0f, 0.0f};
	cam->zoom = DEFAULT_CAMERA2D_ZOOM;
	cam->rotation = DEFAULT_CAMERA2D_ROTATION;
	cam->viewport_size = (vec2){0.0f, 0.0f};
	cam->priority = DEFAULT_CAMERA2D_PRIORITY;
	cam->active = false;
	cam->view = mat4_identity();
	cam->projection = mat4_identity();
	cam->view_dirty = true;
	cam->projection_dirty = true;
}


static void update_view_matrix(camera2d_t* cam) {
	// view = translate(vp/2) × scale(zoom) × rotate(-angle) × translate(-position)
	// read right-to-left: move world so camera is at origin, rotate, scale,
	// then shift by half-viewport so zoom stays centered on the camera position.
	mat4 t1 = mat4_translate(-cam->position.x, -cam->position.y, 0.0f);
	mat4 r  = mat4_rotate_z(-cam->rotation);
	mat4 s  = mat4_scale(cam->zoom, cam->zoom, 1.0f);
	mat4 t2 = mat4_translate(cam->viewport_size.x * 0.5f,
	                         cam->viewport_size.y * 0.5f, 0.0f);

	mat4 rt = mat4_multiply(r, t1);
	mat4 srt = mat4_multiply(s, rt);
	cam->view = mat4_multiply(t2, srt);

	cam->view_dirty = false;
}


static void update_projection_matrix(camera2d_t* cam) {
	// Top-left origin, Y-down: bottom and top are swapped in ortho.
	// Near/far arbitrary for 2D — any non-zero range works.
	cam->projection = mat4_orthographic(0.0f, cam->viewport_size.x,
	                                    cam->viewport_size.y, 0.0f,
	                                    -1.0f, 1.0f);
	cam->projection_dirty = false;
}


void camera2d_init(vec2 ui_virtual_resolution) {
	camera_table.count = 0;
	camera_table.ui_camera = NULL;
	for (uint32_t i = 0; i < MAX_CAMERAS2D; i++) {
		reset_camera(&camera_table.cameras[i]);
	}

	// Create built-in UI camera.
	//
	// The UI camera's position is set to viewport/2, NOT (0,0). Reason:
	// camera2d's view matrix is `translate(vp/2) × scale(zoom) × translate(-position)`
	// — designed so that zooming stays centered on the camera position, which
	// is the correct behaviour for world cameras (player stays centered).
	// For a UI camera we want world (0,0) to map to screen (0,0) — top-left.
	// Setting position = vp/2 makes translate(-position) cancel the
	// translate(vp/2) prefix, so the view becomes identity at zoom=1 and
	// world coordinates match screen coordinates directly.
	camera_table.ui_camera = camera2d_new(
		(vec2){ui_virtual_resolution.x * 0.5f, ui_virtual_resolution.y * 0.5f},
		DEFAULT_CAMERA2D_ZOOM,
		ui_virtual_resolution,
		UI_CAMERA2D_PRIORITY
	);
	if (!camera_table.ui_camera) {
		fprintf(stderr, "camera2d_init: failed to create UI camera\n");
	}
}


void camera2d_shutdown(void) {
	for (uint32_t i = 0; i < MAX_CAMERAS2D; i++) {
		reset_camera(&camera_table.cameras[i]);
	}
	camera_table.count = 0;
	camera_table.ui_camera = NULL;
}


camera2d_t* camera2d_new(vec2 position, float zoom, vec2 viewport_size, int32_t priority) {
	// Find first inactive slot
	for (uint32_t i = 0; i < MAX_CAMERAS2D; i++) {
		if (!camera_table.cameras[i].active) {
			camera2d_t* cam = &camera_table.cameras[i];
			reset_camera(cam);
			cam->position = position;
			cam->zoom = zoom;
			cam->viewport_size = viewport_size;
			cam->priority = priority;
			cam->active = true;
			camera_table.count++;
			return cam;
		}
	}
	fprintf(stderr, "camera2d_new: camera table full (MAX_CAMERAS2D = %d)\n", MAX_CAMERAS2D);
	return NULL;
}


void camera2d_destroy(camera2d_t* cam) {
	if (!cam || !cam->active) return;
	reset_camera(cam);
	camera_table.count--;
}


void camera2d_set_position(camera2d_t* cam, vec2 position) {
	cam->position = position;
	cam->view_dirty = true;
}

void camera2d_set_zoom(camera2d_t* cam, float zoom) {
	cam->zoom = zoom;
	cam->view_dirty = true;
}

void camera2d_set_rotation(camera2d_t* cam, float radians) {
	cam->rotation = radians;
	cam->view_dirty = true;
}

void camera2d_set_priority(camera2d_t* cam, int32_t priority) {
	cam->priority = priority;
	// no matrix impact
}

void camera2d_set_viewport(camera2d_t* cam, vec2 viewport_size) {
	cam->viewport_size = viewport_size;
	cam->view_dirty = true;         // view uses viewport/2 translation
	cam->projection_dirty = true;   // projection uses viewport extents
}


vec2 camera2d_get_position(camera2d_t* cam) {
	return cam->position;
}

float camera2d_get_zoom(camera2d_t* cam) {
	return cam->zoom;
}

float camera2d_get_rotation(camera2d_t* cam) {
	return cam->rotation;
}

int32_t camera2d_get_priority(camera2d_t* cam) {
	return cam->priority;
}

vec2 camera2d_get_viewport(camera2d_t* cam) {
	return cam->viewport_size;
}


camera2d_t* camera2d_get_ui(void) {
	return camera_table.ui_camera;
}


mat4 camera2d_get_view_matrix(camera2d_t* cam) {
	if (cam->view_dirty) update_view_matrix(cam);
	return cam->view;
}

mat4 camera2d_get_projection_matrix(camera2d_t* cam) {
	if (cam->projection_dirty) update_projection_matrix(cam);
	return cam->projection;
}


vec2 camera2d_screen_to_virtual(camera2d_t* cam, float screen_x, float screen_y,
                                 float window_w, float window_h) {
	// Step 1: screen pixels → normalized [0..1] in window
	float nx = screen_x / window_w;
	float ny = screen_y / window_h;

	// Step 2: normalized → camera view space (virtual units, top-left origin)
	float vx = nx * cam->viewport_size.x;
	float vy = ny * cam->viewport_size.y;

	// Step 3: undo the view transform to get world virtual coordinates.
	// view = translate(vp/2) × scale(zoom) × rotate(-angle) × translate(-position)
	// inverse sequence: (vx,vy) - vp/2, scale by 1/zoom, rotate by +angle, + position.
	vx -= cam->viewport_size.x * 0.5f;
	vy -= cam->viewport_size.y * 0.5f;

	float inv_zoom = (cam->zoom != 0.0f) ? 1.0f / cam->zoom : 1.0f;
	vx *= inv_zoom;
	vy *= inv_zoom;

	float c = cosf(cam->rotation);
	float s = sinf(cam->rotation);
	float rx =  c * vx - s * vy;
	float ry =  s * vx + c * vy;

	return (vec2){ rx + cam->position.x, ry + cam->position.y };
}


camera2d_t* camera2d_iter(camera2d_t* prev) {
	uint32_t start = 0;
	if (prev) {
		// compute next index from pointer offset
		start = (uint32_t)(prev - camera_table.cameras) + 1;
	}
	for (uint32_t i = start; i < MAX_CAMERAS2D; i++) {
		if (camera_table.cameras[i].active) return &camera_table.cameras[i];
	}
	return NULL;
}
