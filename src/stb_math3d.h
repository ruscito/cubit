// stb_math3d.h

#ifndef STB_MATH3D_H
#define STB_MATH3D_H

#include <math.h>

/*
 * Matrices are intended column-major:
 * In the linear reppresentation the first
 * four elements of the array are the first
 * column of the matrix and so forth.
 *
 * Logic layout:    Memory linear layout:
 * 0  4  8 12  		[ 0] [ 1] [ 2] [ 3]
 * 1  5  9 13  		[ 4] [ 5] [ 6] [ 7]
 * 2  6 10 14  		[ 8] [ 9] [10] [11]
 * 3  7 11 15  		[12] [13] [14] [15]
 *
 */

// Data type definition
typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { float m[16]; } mat4;


// Declaration
vec2 vec2_add(vec2 a, vec2 b);
vec2 vec2_sub(vec2 a, vec2 b);
vec2 vec2_scale(vec2 a, float s);
float vec2_dot(vec2 a, vec2 b);
vec2 vec2_multiply(vec2 a, vec2 b);
float vec2_length(vec2 a);
vec2 vec2_normalize(vec2 a);

vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_scale(vec3 a, float s);
float vec3_dot(vec3 a, vec3 b);
vec3 vec3_cross(vec3 a, vec3 b);
vec3 vec3_multiply(vec3 a, vec3 b);
float vec3_length(vec3 a);
vec3 vec3_normalize(vec3 a);
vec3 vec3_multiply_mat4(vec3 v, mat4 m);

vec4 vec4_add(vec4 a, vec4 b);
vec4 vec4_sub(vec4 a, vec4 b);
vec4 vec4_scale(vec4 a, float s);
float vec4_dot(vec4 a, vec4 b) ;
float vec4_length(vec4 a);
vec4 vec4_normalize(vec4 a);

mat4 mat4_identity(void);
mat4 mat4_multiply(mat4 a, mat4 b);
mat4 mat4_translate(float x, float y, float z);
mat4 mat4_rotate_x(float r);
mat4 mat4_rotate_y(float r);
mat4 mat4_rotate_z(float r);
mat4 mat4_scale(float sx, float sy, float sz);
mat4 mat4_perspective(float fov, float a, float n, float f);
mat4 mat4_orthographic(float l, float r, float b, float t, float n, float f);
mat4 mat4_look_at(vec3 e, vec3 c, vec3 u);


#ifdef STB_MATH3D_IMPLEMENTATION

/************************** vec2  ***************************/
vec2 vec2_add(vec2 a, vec2 b) {
	return (vec2){ a.x + b.x, a.y + b.y };
}

vec2 vec2_sub(vec2 a, vec2 b) {
	return (vec2){ a.x - b.x, a.y - b.y };
}

vec2 vec2_scale(vec2 a, float s) {
	return (vec2){ a.x * s, a.y * s };
}

float vec2_dot(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}

vec2 vec2_multiply(vec2 a, vec2 b) {
	return (vec2){ a.x * b.x, a.y * b.y };
}

float vec2_length(vec2 a) {
	return sqrtf(a.x * a.x + a.y * a.y);
}

vec2 vec2_normalize(vec2 a) {
	// vector magnitude
	float m = vec2_length(a);
	if (m == 0) return (vec2){ 0.0f, 0.0f };
	return (vec2){ a.x / m, a.y / m };
}

/************************** vec3  ***************************/
vec3 vec3_add(vec3 a, vec3 b) {
	return (vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

vec3 vec3_sub(vec3 a, vec3 b) {
	return (vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

vec3 vec3_scale(vec3 a, float s) {
	return (vec3){ a.x * s, a.y * s, a.z * s};
}

float vec3_dot(vec3 a, vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
	return (vec3){
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

vec3 vec3_multiply(vec3 a, vec3 b) {
    return (vec3){ a.x * b.x, a.y * b.y, a.z * b.z };
}

float vec3_length(vec3 a) {
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

vec3 vec3_normalize(vec3 a) {
	// vector magnitude
	float m =  vec3_length(a);
	if (m == 0) return (vec3) { 0.0f, 0.0f, 0.0f };
	return (vec3) { a.x / m, a.y / m, a.z / m };
}

vec3 vec3_multiply_mat4(vec3 v, mat4 m) {
    vec3 res;
    // Column-major indices:
    // [0] [4] [ 8] [12]  <- Row 0
    // [1] [5] [ 9] [13]  <- Row 1
    // [2] [6] [10] [14]  <- Row 2
    // [3] [7] [11] [15]  <- Row 3 (w)

    res.x = v.x * m.m[0] + v.y * m.m[4] + v.z * m.m[8]  + m.m[12];
    res.y = v.x * m.m[1] + v.y * m.m[5] + v.z * m.m[9]  + m.m[13];
    res.z = v.x * m.m[2] + v.y * m.m[6] + v.z * m.m[10] + m.m[14];

    return res;
}

/************************** vec4 *****************************/
vec4 vec4_add(vec4 a, vec4 b) {
	return (vec4){ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

vec4 vec4_sub(vec4 a, vec4 b) {
	return (vec4){ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w };
}

vec4 vec4_scale(vec4 a, float s) {
	return (vec4){ a.x * s, a.y * s, a.z * s, a.w * s};
}

float vec4_dot(vec4 a, vec4 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float vec4_length(vec4 a) {
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}

vec4 vec4_normalize(vec4 a) {
	// vector magnitude
	float m =  vec4_length(a);
	if (m == 0) return (vec4) { 0.0f, 0.0f, 0.0f, 0.0f };
	return (vec4) { a.x / m, a.y / m, a.z / m, a.w /m};
}

/************************** mat4 ***************************/
mat4 mat4_identity(void) {
    return (mat4){{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    }};
}

mat4 mat4_multiply(mat4 a, mat4 b) {
    mat4 r;
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}

mat4 mat4_translate(float x, float y, float z) {
    return (mat4){{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        x, y, z, 1
    }};
}

mat4 mat4_rotate_x(float r) {
	float c = cosf(r);
	float s = sinf(r);
    return (mat4){{
        1, 0, 0, 0,
        0, c, s, 0,
        0,-s, c, 0,
        0, 0, 0, 1
    }};
}

mat4 mat4_rotate_y(float r) {
	float c = cosf(r);
	float s = sinf(r);
    return (mat4){{
        c, 0,-s, 0,
        0, 1, 0, 0,
        s, 0, c, 0,
        0, 0, 0, 1
    }};
}

mat4 mat4_rotate_z(float r) {
	float c = cosf(r);
	float s = sinf(r);
    return (mat4){{
        c, s, 0, 0,
       -s, c, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    }};
}

mat4 mat4_scale(float sx, float sy, float sz) {
    return (mat4){{
       sx, 0, 0, 0,
        0,sy, 0, 0,
        0, 0,sz, 0,
        0, 0, 0, 1
    }};
}

/*
 * This matrix transform 3D View Space (camera space)
 * in to Clip Space
 * fov: field of view in radiant
 * a: aspect ration width/height
 * n: near clipping distance;
 * f: far clipping distance
 */
mat4 mat4_perspective(float fov, float a, float n, float f) {
	float t = tanf(fov/2);
	mat4 r = mat4_identity();
	r.m[ 0] = 1 / (a * t);
	r.m[ 5] = 1 / t;
	r.m[10] = (n + f) / (n - f);
	r.m[11] = -1;
	r.m[14] = (2 * f * n) / (n - f);
	r.m[15] = 0;
	return r;
}

/*
 * This matrix is used for 2D and UI or isometric
 * left: left coordinate;
 * r: right coordinate;
 * b: bottom coordinate;
 * t: top coordinate;
 * n: near clipping plane;
 * f: far clipping plane
 */
mat4 mat4_orthographic(float l, float r, float b, float t, float n, float f) {
	return (mat4) {{
		    2/(r-l),           0,           0, 0,
				  0,     2/(t-b),           0, 0,
	              0,           0,    -2/(f-n), 0,
	   -(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n), 1
	}};
}

/*
 * This is the view matrix and it transforms coordinates
 * from World Space to Camera Space. It is effectively
 * the inverse of the camera's position in the world.
 * position: eye, the position of the camera
 * direction: the normalized point the camera is looking at
 * up: the world's up direction
 */
mat4 mat4_look_at(vec3 position, vec3 direction, vec3 up) {
    vec3 camera_direction  = direction;
    vec3 camera_right = vec3_normalize(vec3_cross(camera_direction, up));
    vec3 camera_up   = vec3_cross(camera_right, camera_direction);

    float ds = -vec3_dot(camera_right, position);
    float du = -vec3_dot(camera_up, position);
    float df =  vec3_dot(camera_direction, position);

    return (mat4){{
        camera_right.x, camera_up.x, -camera_direction.x, 0,
        camera_right.y, camera_up.y, -camera_direction.y, 0,
        camera_right.z, camera_up.z, -camera_direction.z, 0,
        			ds,     	 du,    			  df, 1
    }};
}

#endif // STB_MATH3D_IMPLEMENTATION

#endif
