// text.c

#include "text.h"
#include "texture.h"
#include "cubit.h"
#include "stb_image.h"
#include "font_atlas.h"

#include <stdlib.h>
#include <stdio.h>

#define MAX_GLYPHS 1024
#define CELL_SIZE  32.0f
#define GLYPH_ADVANCE 0.5f

static mat4 glyph_transforms[MAX_GLYPHS];
static vec4 glyph_uv_rects[MAX_GLYPHS];
static color_t glyph_colors[MAX_GLYPHS];
static uint32_t glyph_count = 0;
static texture_t* font_texture;

static float quad_pos[] = {
	0.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	1.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
};

static uint32_t quad_indices[] = {
	 0,  1,  2,
     2,  3,  0,
};

static float quad_uvs[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
};

static mesh_t* quad;


static vec4 get_uv_rect(char c, int w, int h) {
    if (c < 32 || c > 126) c = ' ';
    float col = ((c - 32) % 16) * 32;
    float row = ((c - 32) / 16) * 32;
    float half_x = 0.5f / w;
    float half_y = 0.5f / h;
    return (vec4){
         col         / w + half_x,    // min_u
         row         / h + half_y,    // min_v
        (col + 32.0f) / w - half_x,   // max_u
        (row + 32.0f) / h - half_y    // max_v
    };
}


/* it create the resource for the default
 * font (testure, mesh and quad) */
void text_init(void) {
    int w, h, ch;
    unsigned char* pixels = stbi_load_from_memory(font_atlas_png, font_atlas_png_size, &w, &h, &ch, 4);

    if (!pixels) {
        fprintf(stderr, "Failed to load default font atlas!\n");
        exit(-1);
    }

    // Now I can create the texture
    font_texture = texture_create_from_memory(pixels, (uint32_t)w, (uint32_t)h, 4, DATA_TEXTURE);
    if (!font_texture) {
        fprintf(stderr, "Failed to create  default font texture!\n");
        stbi_image_free(pixels);
        exit(-1);
    }

    stbi_image_free(pixels);

    quad = mesh_create(quad_pos, NULL, quad_uvs, NULL, quad_indices, 4, 6);

    if (!quad) {
        fprintf(stderr, "Failed to create  default font quad!\n");
        exit(-1);
    }
}

void text_shutdown(void) {
    mesh_destroy(quad);
    texture_destroy(font_texture);
}

texture_t* text_get_default_font_texture(void) {
    return font_texture;
}

/* Submit a text string for rendering in the 2D overlay pass.
 * Glyphs are queued into an internal buffer and drawn at the end
 * of the frame by the backend using an orthographic projection.
 *
 * text   - null-terminated string to render
 * length - number of characters to render
 * size   - scale factor (1.0 = 32px cell size)
 * x, y   - screen position in pixels (top-left origin)
 * color  - per-glyph tint color (multiplied with atlas texel)  */
void text_draw(const char* text, uint32_t length, float size, uint32_t x, uint32_t y, color_t color) {
    float scaled = size * CELL_SIZE;
    float cursor_x = (float)x;
    float cursor_y = (float)y;

    for (uint32_t i = 0; i < length; i++) {
        if (glyph_count >= MAX_GLYPHS) break;

        vec4 uv = get_uv_rect(text[i], font_texture->width, font_texture->height);

        mat4 transform = mat4_identity();
        transform.m[0]  = scaled;       // scale X
        transform.m[5]  = scaled;       // scale Y
        transform.m[12] = cursor_x;     // translate X
        transform.m[13] = cursor_y;     // translate Y

        glyph_transforms[glyph_count] = transform;
        glyph_uv_rects[glyph_count] = uv;
        glyph_colors[glyph_count] = color;
        glyph_count++;

        cursor_x += scaled * 0.5f;
    }
}

mat4* text_get_transforms(void) {
    return glyph_transforms;
}

vec4* text_get_uv_rects(void) {
    return glyph_uv_rects;
}

uint32_t text_get_count(void) {
    return glyph_count;
}

mesh_t* text_get_quad(void) {
    return quad;
}

texture_t* text_get_texture(void) {
    return font_texture;
}

void text_reset(void) {
    glyph_count = 0;
}

color_t* text_get_colors(void) {
    return glyph_colors;
}
