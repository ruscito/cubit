// material2d.c

#include "material2d.h"
#include "texture.h"
#include "shader.h"

#include <stdlib.h>
#include <stdio.h>


static material2d_t* default_material2d;


void material2d_init(void) {
	default_material2d = material2d_create();
	if (default_material2d == NULL) {
		fprintf(stderr, "Failed to create default material2d\n");
		exit(-1);
	}
}

void material2d_shutdown(void) {
	material2d_destroy(default_material2d);
	default_material2d = NULL;
}

material2d_t* material2d_get_default(void) {
	return default_material2d;
}

material2d_t* material2d_create(void) {
	material2d_t* m = malloc(sizeof(*m));
	if (m == NULL) {
		fprintf(stderr, "Failed to create material2d\n");
		return NULL;
	}
	m->color = COLOR_WHITE;
	m->diffuse_texture = texture_get_default(DIFFUSE);
	m->shader = shader_get_sprite();
	m->blend_mode = BLEND_NORMAL;
	return m;
}

void material2d_destroy(material2d_t* m) {
	if (!m) return;
	free(m);
}

void material2d_set_color(material2d_t* m, color_t c) {
	m->color = c;
}

void material2d_set_diffuse_texture(material2d_t* m, texture_t* t) {
	m->diffuse_texture = t;
}

void material2d_set_shader(material2d_t* m, shader_t* s) {
	m->shader = s;
}

void material2d_set_blend_mode(material2d_t* m, blend_mode_t mode) {
	m->blend_mode = mode;
}
