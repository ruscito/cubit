// camera.c

#include "cubit.h"
#include "camera.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>

static int32_t id = 0;


// Internal helper functions
static void update_direction_from_angles(camera_t *c);
static void update_direction_from_target(camera_t *c);
static void update_view_matrix(camera_t *c);
static void update_projection_matrix(camera_t *c);
static void update_view_projection(camera_t *c);
static void update_frustum(camera_t *c);

static vec4 normalize_plane(vec4 p);


camera_t* camera_target_new(int32_t projection_mode, vec3 position, vec3  up,
		                    float fov, float near, float far, float aspect,
							vec3 target) {
	camera_t *c = malloc(sizeof(*c));
	if (c==NULL) {
		fprintf(stderr, "Failed to allocate camera object\n");
		exit(-1);
	}
	c->id = id++;
	c->ortho_size = DEFAULT_ORTHO_SIZE;
	c->projection_mode = projection_mode;
	c->camera_mode = CAMERA_MODE_TARGET;
	c->position = position;
 	c->up = up;
	c->fov = fov;
	c->base_fov = fov;
	c->zoom_factor = 1.0f;
	c->near = near;
	c->far = far;
	c->aspect = aspect;

	c->target = target;
	update_direction_from_target(c);
	c->pitch = asinf(c->direction.y);
	c->yaw = atan2f(c->direction.z, c->direction.x);
	c->view_dirty = true;
	c->projection_dirty = true;
	return c;
};


camera_t* camera_free_new(int32_t projection_mode, vec3 position, vec3  up,
		                  float fov, float near, float far, float aspect,
						  float yaw, float pitch) {
	camera_t *c = malloc(sizeof(*c));
	if (c==NULL) {
		fprintf(stderr, "Failed to allocate camera object\n");
		exit(-1);
	}
	c->id = id++;
	c->ortho_size = DEFAULT_ORTHO_SIZE;
	c->projection_mode = projection_mode;
	c->camera_mode = CAMERA_MODE_FREE;
	c->position = position;
 	c->up = up;
	c->fov = fov;
	c->base_fov = fov;
	c->zoom_factor = 1.0f;
	c->near = near;
	c->far = far;
	c->aspect = aspect;

	c->yaw = yaw;
	c->pitch = pitch;
    update_direction_from_angles(c);
	c->view_dirty = true;
	c->projection_dirty = true;
    // Set as default
	return c;
};


void camera_update(camera_t* c) {
	bool needs_vp_update = c->view_dirty || c->projection_dirty;
	if (c->view_dirty) {
		update_view_matrix(c);
		c->view_dirty = false;
	}
	if (c->projection_dirty) {
		update_projection_matrix(c);
		c->projection_dirty = false;
	}

	if (needs_vp_update) {
		update_view_projection(c);
		update_frustum(c);
	}
}


void camera_destroy(camera_t* c) {
	if (c) free(c);
}

void camera_move_forward(camera_t* c, float distance) {
    if (c->camera_mode == CAMERA_MODE_FREE) {
        // Move along camera's forward direction
        vec3 forward = c->direction;
        c->position = vec3_add(c->position, vec3_scale(forward, distance));
    } else {
        // In target mode, move along the direction to target
        vec3 forward = c->direction;
        c->position = vec3_add(c->position, vec3_scale(forward, distance));
        // Note: target stays fixed, so direction changes
        update_direction_from_target(c);
    }
	c->view_dirty = true;
}

void camera_move_right(camera_t* c, float distance) {
    vec3 right = vec3_normalize(vec3_cross(c->direction, c->up));
    c->position = vec3_add(c->position, vec3_scale(right, distance));

    if (c->camera_mode == CAMERA_MODE_TARGET) {
        update_direction_from_target(c);
    }
	c->view_dirty = true;
}

void camera_move_up(camera_t* c, float distance) {
    c->position = vec3_add(c->position, vec3_scale(c->up, distance));

    if (c->camera_mode == CAMERA_MODE_TARGET) {
        update_direction_from_target(c);
    }
	c->view_dirty = true;
}

void camera_rotate_yaw(camera_t* c, float angle) {
    // Switch to FREE mode if in TARGET mode
    if (c->camera_mode == CAMERA_MODE_TARGET) {
        c->camera_mode = CAMERA_MODE_FREE;
    }

    c->yaw += angle;
    update_direction_from_angles(c);
	c->view_dirty = true;
}

void camera_rotate_pitch(camera_t* c, float angle) {
    // Switch to FREE mode if in TARGET mode
    if (c->camera_mode == CAMERA_MODE_TARGET) {
        c->camera_mode = CAMERA_MODE_FREE;
    }

    c->pitch += angle;

    // Clamp pitch to prevent gimbal lock
    float limit = DEFAULT_PITCH_LIMIT;
    if (c->pitch > limit) c->pitch = limit;
    if (c->pitch < -limit) c->pitch = -limit;

    update_direction_from_angles(c);
	c->view_dirty = true;
}

void camera_set_target(camera_t* c, vec3 target) {
    c->camera_mode = CAMERA_MODE_TARGET;
    c->target = target;
    update_direction_from_target(c);

    // Update yaw/pitch for potential mode switching
    c->pitch = asinf(c->direction.y);
    c->yaw = atan2f(c->direction.z, c->direction.x);

	c->view_dirty = true;
}

void camera_set_orbit(camera_t* c, float yaw_delta, float pitch_delta) {
    if (c->camera_mode == CAMERA_MODE_FREE) {
        // Switch to target mode using current look point
        vec3 look_point = vec3_add(c->position, c->direction);
        c->target = look_point;
        c->camera_mode = CAMERA_MODE_TARGET;
    }

    // Calculate current distance to target
    vec3 to_target = vec3_sub(c->target, c->position);
    float distance = vec3_length(to_target);

    // Update angles
    c->yaw += yaw_delta;
    c->pitch += pitch_delta;

    // Clamp pitch
    float limit = 89.0f * DEG2RAD;
    if (c->pitch > limit) c->pitch = limit;
    if (c->pitch < -limit) c->pitch = -limit;

    // Calculate new position orbiting around target
    update_direction_from_angles(c);
    vec3 offset = vec3_scale(c->direction, -distance);
    c->position = vec3_add(c->target, offset);

    update_direction_from_target(c);
	c->view_dirty = true;
}

void camera_set_aspect_ratio(camera_t* c,float aspect) {
    c->aspect = aspect;
	c->projection_dirty = true;
}

void camera_set_projection_mode(camera_t* c, int32_t mode) {
    c->projection_mode = mode;
	c->projection_dirty = true;
}

void camera_set_fov(camera_t* c, float fov) {
	c->fov = fov;
	c->base_fov = fov;
	c->zoom_factor = 1.0f;
	c->projection_dirty = true;
}

float camera_get_fov(camera_t* c) {
	return c->fov;
}

void camera_zoom(camera_t* c, float factor) {
	if (factor < 1.0) factor = 1.0f;
	c->zoom_factor = factor;
	c->fov = c->base_fov/factor;

	// Clamp
	if (c->fov < MIN_FOV) {
		c->fov = MIN_FOV;
		c->zoom_factor = c->base_fov / MIN_FOV;
	}
	if (c->fov > MAX_FOV) {
		c->fov = MAX_FOV;
		c->zoom_factor = c->base_fov / MAX_FOV;
	}
	c->projection_dirty = true;
}

void camera_smooth_zoom(camera_t* c, float target_factor, float speed, double dt) {
    // Exponential decay: smoothing_factor = 1 - e^(-speed * dt)
    float t = 1.0f - expf(-speed * (float)dt);
    c->zoom_factor = lerp_precise(c->zoom_factor, target_factor, t);
    camera_zoom(c, c->zoom_factor);
}

static void update_direction_from_angles(camera_t* c) {
    c->direction.x = cosf(c->pitch) * cosf(c->yaw);
    c->direction.y = sinf(c->pitch);
    c->direction.z = cosf(c->pitch) * sinf(c->yaw);
    c->direction = vec3_normalize(c->direction);
}

static void update_direction_from_target(camera_t* c) {
    c->direction = vec3_normalize(vec3_sub(c->target, c->position));
}

static void update_view_matrix(camera_t* c) {
    c->v = mat4_look_at(c->position, c->direction, c->up);
}

static void update_projection_matrix(camera_t* c) {
    if (c->projection_mode == PERSPECTIVE) {
        c->p = mat4_perspective(c->fov, c->aspect, c->near, c->far);
    } else {
        // Orthographic projection
        float height = c->ortho_size;
        float width = height * c->aspect;
        c->p = mat4_orthographic(-width, width, -height, height, c->near, c->far);
    }
}

static void update_view_projection(camera_t* c) {
    c->vp = mat4_multiply(c->p, c->v);
}

static vec4  normalize_plane(vec4 p) {
	float len = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
	if (len == 0) return (vec4) { 0.0f, 0.0f, 0.0f, 0.0f };
	return (vec4) { p.x / len, p.y / len, p.z / len, p.w / len };
}

static void update_frustum(camera_t* c) {
	vec4 row3 = (vec4){c->vp.m[3], c->vp.m[7], c->vp.m[11], c->vp.m[15]};
	c->left_plane   = normalize_plane(vec4_add(row3, (vec4){
				c->vp.m[0],c->vp.m[4], c->vp.m[8], c->vp.m[12]}));
	c->right_plane  = normalize_plane(vec4_sub(row3, (vec4){
				c->vp.m[0],c->vp.m[4], c->vp.m[8], c->vp.m[12]}));
	c->bottom_plane = normalize_plane(vec4_add(row3, (vec4){
				c->vp.m[1],c->vp.m[5], c->vp.m[9], c->vp.m[13]}));
	c->top_plane    = normalize_plane(vec4_sub(row3, (vec4){
				c->vp.m[1],c->vp.m[5], c->vp.m[9], c->vp.m[13]}));
	c->near_plane   = normalize_plane(vec4_add(row3, (vec4){
				c->vp.m[2],c->vp.m[6], c->vp.m[10], c->vp.m[14]}));
	c->far_plane    = normalize_plane(vec4_sub(row3, (vec4){
				c->vp.m[2],c->vp.m[6], c->vp.m[10], c->vp.m[14]}));
}

mat4 camera_get_view_projection_matrix(camera_t* c) {
    return c->vp;
}

mat4 camera_get_view_matrix(camera_t* c) {
    return c->v;
}

mat4 camera_get_projection_matrix(camera_t* c) {
    return c->p;
}

vec3 camera_get_position(camera_t* c) {
    return c->position;
}

vec3 camera_get_direction(camera_t* c) {
    return c->direction;
}

int32_t  camera_get_mode(camera_t* c) {
    return c->camera_mode;
}

void camera_set_ortho_size(camera_t* c, float size) {
	c->ortho_size = size;
	c->projection_dirty = true;
}

float camera_get_ortho_size(camera_t* c) {
	return c->ortho_size;
}

vec3 camera_get_right(camera_t* c) {
	return vec3_normalize(vec3_cross(c->direction, c->up));
}

vec3 camera_get_up(camera_t* c) {
	return vec3_cross(camera_get_right(c), c->direction);
}

void camera_update_viewport(camera_t* c, int width, int height) {
	camera_set_aspect_ratio(c, (float)width/(float)height);
}

float camera_get_zoom_factor(camera_t* c) {
    return c->zoom_factor;
}


static bool is_aabb_inside_plane(aabb_t aabb, vec4 plane, float border) {
	float px = plane.x >= 0 ? aabb.max_x : aabb.min_x;
	float py = plane.y >= 0 ? aabb.max_y : aabb.min_y;
	float pz = plane.z >= 0 ? aabb.max_z : aabb.min_z;
	return (plane.x * px + plane.y * py + plane.z * pz + (plane.w + border)) >= 0;
}

/* Return true is the AABB of the object passed is within th camera planes.
 * The function internally add a border to the Z axes of each planes to
 * avoid object desappearing from the camera view */
bool camera_is_object3d_visible(camera_t* c, object3d_t* o) {
	aabb_t aabb = object3d_get_aabb(o);
	return is_aabb_inside_plane(aabb, c->left_plane, CULLING_BORDER) &&
		   is_aabb_inside_plane(aabb, c->right_plane, CULLING_BORDER) &&
		   is_aabb_inside_plane(aabb, c->top_plane, CULLING_BORDER) &&
		   is_aabb_inside_plane(aabb, c->bottom_plane, CULLING_BORDER) &&
		   is_aabb_inside_plane(aabb, c->near_plane, CULLING_BORDER) &&
		   is_aabb_inside_plane(aabb, c->far_plane, CULLING_BORDER);
}

/* Get a camera and calculate the 8 corners of the frustum. The 4 near plane
 * coners and the 4 far plane cornes. The 8 corners are in world space
 * coordinates. */
void camera_get_frustum_corners(camera_t* c, vec3* corners, float near_dist, float far_dist) {
    // Calculating half-height and half-width
    // of near and far plane
    float t = tanf(c->fov/2.0f);
    float n_half_height = t * near_dist;
    float n_half_width = n_half_height * c->aspect;
    float f_half_height = t * far_dist;
    float f_half_width = f_half_height * c->aspect;

    // Finding the center of the near and far plane
    vec3 n_center = vec3_add(c->position, vec3_scale(c->direction, near_dist));
    vec3 f_center = vec3_add(c->position, vec3_scale(c->direction, far_dist));

    // Building corners
    vec3 right = camera_get_right(c);
    vec3 up = camera_get_up(c);
    // near up-right
    corners[0] = vec3_add(n_center, vec3_add(vec3_scale(up, n_half_height),
                vec3_scale(right, n_half_width)));
    // near up-left
    corners[1] = vec3_add(n_center, vec3_add(vec3_scale(up, n_half_height),
                vec3_scale(right, -n_half_width)));
    // near down-right
    corners[2] = vec3_add(n_center, vec3_add(vec3_scale(up, -n_half_height),
                vec3_scale(right, n_half_width)));
    // near down-left
    corners[3] = vec3_add(n_center, vec3_add(vec3_scale(up, -n_half_height),
                vec3_scale(right, -n_half_width)));
    // far up-right
    corners[4] = vec3_add(f_center, vec3_add(vec3_scale(up, f_half_height),
                vec3_scale(right, f_half_width)));
    // far up-left
    corners[5] = vec3_add(f_center, vec3_add(vec3_scale(up, f_half_height),
                vec3_scale(right, -f_half_width)));
    // far down-right
    corners[6] = vec3_add(f_center, vec3_add(vec3_scale(up, -f_half_height),
                vec3_scale(right, f_half_width)));
    // far down-left
    corners[7] = vec3_add(f_center, vec3_add(vec3_scale(up, -f_half_height),
                vec3_scale(right, -f_half_width)));
}



