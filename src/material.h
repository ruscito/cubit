// material.h

#ifndef MATERIAL_H_
#define MATERIAL_H_

#include "cubit_types.h"

struct material_t{
	color_t surface_color;		// Base surface color. Default white
	color_t specular_color;		// Highlight color used once the lighiting lands.
								// Default white
	float shininess; 			// Esponent for the specular color (2-256)
								// Used once lighting lands. Default 32.0f
	float roughness;			// Surface roughness (0-1). Default .5f
								// RESERVED FOR  FUTURE.
	color_t emissive_color; 	// Self-glow, default black
	texture_t *diffuse_texture;	// NULL = no texture
	texture_t *normal_texture;  // NULL = no texture
	shader_t *shader;			// pointer to the shader associated to the material

};

void material_init(void);
void material_shutdown(void);
material_t* material_get_default(void);

#endif

