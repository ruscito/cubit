// texture.c

#include "texture.h"
#include "backend.h"
#include "stb_image.h"

#include <stdlib.h>
#include <stdio.h>

texture_t* default_texture;
texture_t* default_normal_texture;




texture_t* texture_create_from_memory(unsigned char* pixels, uint32_t w,
                                      uint32_t h, uint32_t c, TextureTypes typ) {
	texture_t *t = malloc(sizeof(*t));
	if (!t) {
		fprintf(stderr, "Failed to allocate memory for texture object\n");
		return NULL;
	}

	t->min_filter = NEAREST;
	t->mag_filter = NEAREST;
	t->s_wrap = REPEAT;
	t->t_wrap = REPEAT;

    t->channels = c;
	t->height = h;
	t->width = w;
    t->type = typ;

	backend_texture_new(t, pixels);

    return t;
}

texture_t* texture_create(const char* filename, TextureTypes typ) {
	int width, height, channels;

    unsigned char *raw_pixel = stbi_load(filename, &width, &height, &channels, 0);
	if (!raw_pixel) {
		fprintf(stderr, "Failed to load file: %s\n", filename);
		return NULL;
	}

	texture_t *t = texture_create_from_memory(raw_pixel, width, height, channels, typ);
	stbi_image_free(raw_pixel);

	return t;
}

void texture_destroy(texture_t* t) {
	backend_texture_destroy(t);
	free(t);
}

static void setup_texture(texture_t *texture, TextureTypes t) {
	texture->min_filter = NEAREST;
	texture->mag_filter = NEAREST;
	texture->s_wrap = REPEAT;
	texture->t_wrap = REPEAT;
	texture->width = 1;
	texture->height = 1;
    texture->type = t;
}

static void generate_texture(texture_t *texture, unsigned char *raw_pixels, uint32_t size) {
	texture->channels = size;
	backend_texture_new(texture, raw_pixels);
}

void texture_init(void) {
	// default texture
	default_texture = malloc(sizeof(*default_texture));
	if (!default_texture) {
		fprintf(stderr, "Failed to allocate memory for the default texture\n");
		exit(-1);
	}

	// default normal texture
	default_normal_texture = malloc(sizeof(*default_normal_texture));
	if (!default_normal_texture) {
		fprintf(stderr, "Failed to allocate memory for the default normal texture\n");
		exit(-1);
	}

	setup_texture(default_texture, COLOR_TEXTURE);
	generate_texture(default_texture,  (unsigned char []) {255, 255, 255, 255}, 4);

	setup_texture(default_normal_texture, DATA_TEXTURE);
	generate_texture(default_normal_texture, (unsigned char []) {128, 128, 255}, 3);
}

void texture_shutdown(void) {
	texture_destroy(default_texture);
	texture_destroy(default_normal_texture);
}

texture_t* texture_get_default(uint32_t t) {
	switch (t) {
		case NORMAL:
			return default_normal_texture;
			break;
		case DIFFUSE:
			return default_texture;
			break;
		default:
		    return NULL;
	}
}
