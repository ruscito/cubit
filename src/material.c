// material.c
#include "cubit.h"
#include "material.h"
#include "shader.h"
#include "texture.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glad/glad.h>

#define DEFAULT_SHININESS	32.0f
#define DEFAULT_ROUGHNESS	0.5f

material_t* default_material;


void material_init(void) {
	default_material = material_create();
	if (default_material == NULL) {
		fprintf(stderr, "Failed to create default material\n");
		exit(-1);
	}
}

void material_shutdown(void) {
	material_destroy(default_material);
}

material_t* material_get_default(void) {
	return default_material;
}

material_t* material_create(void) {
	material_t* m = malloc(sizeof(*m));
	if (m == NULL) {
		fprintf(stderr, "Failed to create material\n");
		return NULL;
	}
	m->surface_color = COLOR_WHITE;
	m->specular_color = COLOR_WHITE;
	m->shininess = DEFAULT_SHININESS;
	m->roughness = DEFAULT_ROUGHNESS;
	m->emissive_color = COLOR_BLACK;
	m->diffuse_texture = texture_get_default(DIFFUSE);
	m->normal_texture = texture_get_default(NORMAL);
    m->opacity = 1;
    m->cast_shadow = true;

	m->shader = shader_get_default();
	return m;
}

void material_destroy(material_t* material) {
	if (!material) return;
	free(material);
}

void material_set_cast_shadow(material_t* m, bool v) {
    m->cast_shadow = v;
}

void material_set_surface_color(material_t* m, color_t c) {
	m->surface_color = c;
}

void material_set_specular_color(material_t* m, color_t c) {
	m->specular_color = c;
}

void material_set_shininess(material_t* m, float value) {
	m->shininess = value;
}

void material_set_roughness(material_t* m, float value) {
	m->roughness = value;
}

void material_set_emissive_color(material_t* m, color_t c) {
	m->emissive_color = c;
}

void material_set_diffuse_texture(material_t* m, texture_t* t) {
	m->diffuse_texture = t;
}

void material_set_normal_texture(material_t* m, texture_t* t) {
	m->normal_texture = t;
}

void material_set_opacity(material_t* m, float o) {
    m->opacity = o;
}




