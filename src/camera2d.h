// camera2d.h

#ifndef CAMERA2D_H_
#define CAMERA2D_H_

#include "cubit_types.h"


#define MAX_CAMERAS2D 8
#define DEFAULT_CAMERA2D_ZOOM     1.0f
#define DEFAULT_CAMERA2D_ROTATION 0.0f
#define DEFAULT_CAMERA2D_PRIORITY 0
#define UI_CAMERA2D_PRIORITY      1000


struct camera2d_t {
	vec2 position;          // camera position in virtual units
	float zoom;             // 1.0 = no zoom, 2.0 = zoomed in 2x
	float rotation;         // radians, around the camera center

	vec2 viewport_size;     // virtual units (what the camera sees)

	int32_t priority;       // render order (higher = drawn later / on top)

	bool active;            // slot occupied — false means free to reuse

	// cached
	mat4 view;
	mat4 projection;
	bool view_dirty;
	bool projection_dirty;
};


void camera2d_init(vec2 ui_virtual_resolution);
void camera2d_shutdown(void);

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

mat4 camera2d_get_view_matrix(camera2d_t* cam);
mat4 camera2d_get_projection_matrix(camera2d_t* cam);

vec2 camera2d_screen_to_virtual(camera2d_t* cam, float screen_x, float screen_y,
                                 float window_w, float window_h);

// Iteration for the batch renderer. Returns the next active camera after 'prev',
// or NULL when iteration ends. Pass NULL as 'prev' to start. Order is not sorted
// by priority — the backend is expected to collect then sort as needed.
camera2d_t* camera2d_iter(camera2d_t* prev);


#endif
