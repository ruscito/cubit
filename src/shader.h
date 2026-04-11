// shader.h

#ifndef SHADER_H_
#define SHADER_H_

#include "cubit_types.h"
#include "light.h"

#define MAX_UNIFORMS    16

struct uniform_table_entry_t{
	char*	name;		// Uniform name as written in the glsl shader
	int32_t location;	// OpenGL location returned by glGetUniformLocation
	UniformTypes type;
} ;

typedef struct {
	// Lighting uniform location
	int32_t light_type;
	int32_t light_color;
	int32_t light_intensity;
	int32_t light_direction;
	int32_t light_position;
	int32_t constant_attenuation;
	int32_t linear_attenuation;
	int32_t quadratic_attenuation;
	int32_t cone_inner_cutoff;
	int32_t cone_outer_cutoff;
} light_uniforms_t;


/*
 * Cached uniform locations for built-in shaders.
 * Resolved once at init, used every frame. A location
 * of -1 means the uniform doesn't exist in that shader
 * and should be skipped.
 */
struct builtin_locations_t {
	int32_t vp;
	// Material uniform locations
	int32_t surface_color;
	int32_t specular_color;
	int32_t shininess;
	int32_t emissive_color;
    int32_t opacity;
	// Light uniform locations
	light_uniforms_t light[MAX_LIGHTS];
	int32_t light_count;
	int32_t camera_position;
	int32_t ambient_factor;
	// Texture
	int32_t diffuse_texture;
	int32_t normal_texture;
	// Shadow map
	int32_t shadow_atlas;               // The atlas of all the shadows
    int32_t light_vp[MAX_LIGHTS];       // MAX_LIGHTS per frame can have VP
    int32_t shadow_rect[MAX_LIGHTS];    // MAX_LIGHT shadow tile per frame:
                                        // though shadow_atlas can contains up MAX_SHADOW_TILES
                                        // ony MAX_LIGHTS can be active per frame
};


struct shader_t{
	uint32_t program_id;
	uniform_table_entry_t uniform_table[MAX_UNIFORMS];
	uint32_t uniform_count;
	builtin_locations_t locations;
};

void shader_init(void);
void shader_shutdown(void);
shader_t* shader_get_default(void);
shader_t* shader_get_unlit(void);
shader_t* shader_get_shadow(void);

#endif
