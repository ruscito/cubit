// camera.h

#ifndef CAMERA_H_
#define CAMERA_H_

#include "cubit_types.h"

#define DEFAULT_ORTHO_SIZE 10
#define DEFAULT_PITCH_LIMIT (89.0f * DEG2RAD)
#define MIN_FOV  (10.0f * DEG2RAD)
#define MAX_FOV (120.0f * DEG2RAD)
#define CULLING_BORDER 1.5f


struct camera_t {
	uint32_t id;		// camera id
	vec3 position;		// camera position
	vec3 target;		// used in CAMERA_TARGET_MODE
	vec3 up;			// world up
	vec3 direction; 	// direction camera is looking

	float fov;			// perspective only
	float zoom_factor;
	float base_fov;
	float near;
	float far;
	float aspect; 		// aspect ratio
	float pitch;
	float yaw;

	float ortho_size;  	// for orthigraphics projection
	int32_t projection_mode;
	int32_t camera_mode;

	mat4 p;				// projection matrix
	mat4 v;				// view matrix
	mat4 vp;			// view projection matrix
	bool view_dirty;
	bool projection_dirty;
	vec4 left_plane;
	vec4 right_plane;
	vec4 bottom_plane;
	vec4 top_plane;
	vec4 near_plane;
	vec4 far_plane;
};


void camera_update(camera_t* c);
void camera_set_fov(camera_t* c, float fov);
float camera_get_fov(camera_t* c);
void camera_set_ortho_size(camera_t* c, float size);
float camera_get_ortho_size(camera_t* c);
vec3 camera_get_right(camera_t* c);  // For strafing
vec3 camera_get_up(camera_t* c);     // For vertical movement
mat4 camera_get_view_projection_matrix(camera_t* c);
mat4 camera_get_view_matrix(camera_t* c);
mat4 camera_get_projection_matrix(camera_t* c);
vec3 camera_get_position(camera_t* c);
vec3 camera_get_direction(camera_t* c);
int32_t camera_get_mode(camera_t* c);
float camera_get_zoom_factor(camera_t* c);
bool camera_is_object3d_visible(camera_t* c, object3d_t* o);
void camera_get_frustum_corners(camera_t* c, vec3* corners, float near_dist, float far_dist);


#endif

