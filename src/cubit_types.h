// cubit_types.h

#include <stdint.h>
#include <stdbool.h>

#include "stb_math3d.h"
#include "input.h"

#define PI 3.1415926535f
#define DEG2RAD (PI/180.0f)
#define RAD2DEG (180.0f/PI)
#define TWO_PI (2.0f * PI)


#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H

#define CUBIT_FAIL -1
#define CUBIT_OK	0

#define COLOR_BUFFER 0x01
#define STENCIL_BUFFER 0x02
#define DEPTH_BUFFER 0x03

#define R_CMD_CLEAR 1
#define R_CMD_DRAW 2
#define R_CMD_DRAW_INSTANCED 3
#define R_CMD_VIEWPORT 4
#define R_CMD_STATE 5

#define PERSPECTIVE 0
#define ORTHOGRAPHIC 1

#define UNUSED_ARG(arg) ((void)(arg))

#define CAMERA_MODE_TARGET 0	// Orbits/looks at a target point
#define CAMERA_MODE_FREE   1	// Free roation using yaw/pitch
#define MAX_CASCADES       3

typedef enum {
    COLOR_TEXTURE,
    DATA_TEXTURE
} TextureTypes;

typedef enum {
    OBJECT_2D,
    OBJECT_3D
} ObjectType;

typedef enum {
	UNIFORM_FLOAT,
	UNIFORM_VEC3,
	UNIFORM_MAT4,
	UNIFORM_COUNT,
} UniformTypes;

typedef enum {
	LOC_POSITION,
	LOC_NORMAL,
	LOC_UV,
	LOC_TANGENT,
	LOC_BONE_IDX,
	LOC_BONE_WGHT,
	LOC_MODEL,
    LOC_UV_RECT = 10,
} LocationTypes;

typedef enum {
    LIGHT_OFF,
    LIGHT_DIRECTIONAL,
    LIGHT_POINT,
    LIGHT_SPOT,
} LightTypes;

typedef enum {
	NEAREST,
	LINEAR,
} FilteringMode;

typedef enum {
	REPEAT,
	CLAMP,
} WrappingMode;


typedef struct { float r, g, b, a; } color_t;
typedef struct { float x, y, w, h; } viewport_t;
typedef struct { uint32_t x, y, w, h; } rect_t;

typedef struct camera_t camera_t;
typedef struct object3d_t object3d_t;
typedef struct material_t material_t;
typedef struct uniform_table_entry_t uniform_table_entry_t;
typedef struct builtin_locations_t builtin_locations_t;
typedef struct shader_t shader_t;
typedef struct light_t light_t;
typedef struct mesh_t mesh_t;
typedef struct texture_t texture_t;
typedef struct shadow_tile_t shadow_tile_t;
typedef struct shadow_atlas_t shadow_atlas_t;

typedef struct {
    object3d_t* object;
    vec3 point;
    float distance;
    bool hit;
} raycast_3d_result_t;




#define COLOR_BLACK   (color_t){0.0f, 0.0f, 0.0f, 1.0f}
#define COLOR_WHITE   (color_t){1.0f, 1.0f, 1.0f, 1.0f}
#define COLOR_RED     (color_t){1.0f, 0.0f, 0.0f, 1.0f}
#define COLOR_GREEN   (color_t){0.0f, 1.0f, 0.0f, 1.0f}
#define COLOR_BLUE    (color_t){0.0f, 0.0f, 1.0f, 1.0f}
#define COLOR_GRAY    (color_t){0.5f, 0.5f, 0.5f, 1.0f}
#define COLOR_YELLOW  (color_t){1.0f, 1.0f, 0.0f, 1.0f}

// CUBIT color
#define CUBIT_WHITE   (color_t){0.933f, 0.933f, 0.933f, 1.0f}
#define CUBIT_YELLOW  (color_t){1.000f, 0.843f, 0.000f, 1.0f}
#define CUBIT_GREEN   (color_t){0.686f, 0.843f, 0.373f, 1.0f}
#define CUBIT_BLUE    (color_t){0.529f, 0.686f, 0.843f, 1.0f}
#define CUBIT_RED     (color_t){1.000f, 0.373f, 0.373f, 1.0f}
#define CUBIT_GRAY    (color_t){0.463f, 0.463f, 0.463f, 1.0f}
#define CUBIT_BROWN   (color_t){0.843f, 0.529f, 0.000f, 1.0f}
#define CUBIT_BLACK   (color_t){0.110f, 0.110f, 0.110f, 1.0f}

typedef struct {float a, b, c, d; } plan_t;
typedef struct {float min_x, max_x, min_y, max_y, min_z, max_z; } aabb_t;

typedef struct {
	int width;
	int height;
	char* title;
	int fps;
    uint32_t shadow_atlas_size;
    uint32_t shadow_tile_size;
} app_config_t;

/*
 * Input callbacks definition to be implemented in the API client (the game)
 * to intercept input events coming from mouse or/and keboard. These events
 * are set in the input module but fired by the backend layer.
 */
typedef void (*process_keyboard_func)(int32_t key, int32_t scancode, int32_t action, int32_t mods);
typedef void (*process_mouse_position_func)(double x_pos, double y_pos);
typedef void (*process_mouse_button_func)(int32_t button, int32_t action, int32_t mods);
typedef void (*process_mouse_scroll_func)(double x_offset, double y_offset);
typedef void (*process_mouse_enter_func)(int32_t entered);
typedef void (*viewport_resized_func)(int32_t width, int32_t height);


#endif
