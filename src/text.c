// text.c
//
// Text rendering as a thin layer on top of sprite_submit().
// One sprite_submit() call per glyph, one shared material2d_t with the font
// atlas as diffuse texture. Everything else (batching, sorting, draw call
// emission) is the 2D pipeline's job.
//
// What this module owns:
//   - the font atlas texture (loaded from the embedded font_atlas.png)
//   - one material2d_t referencing that texture
//
// What this module does NOT own anymore (refactored away in stage 22 step 9):
//   - per-glyph buffers (transforms, uv_rects, colors)
//   - a quad mesh
//   - any custom GL state

#include "text.h"
#include "texture.h"
#include "material2d.h"
#include "camera2d.h"
#include "cubit.h"
#include "stb_image.h"
#include "font_atlas.h"

#include <stdio.h>
#include <stdlib.h>


#define CELL_SIZE      32.0f
#define GLYPH_ADVANCE  0.5f


static texture_t*   font_texture;
static material2d_t* font_material;


// Computes the atlas sub-rect for a printable ASCII char, with a half-pixel
// inset on every side to avoid bilinear sampling bleed from neighboring
// glyphs. The atlas layout is fixed: 16 columns × N rows of 32×32 cells,
// starting at ASCII 32 (space).
static vec4 get_uv_rect(char c, int w, int h) {
	if (c < 32 || c > 126) c = ' ';
	float col = ((c - 32) % 16) * 32;
	float row = ((c - 32) / 16) * 32;
	float half_x = 0.5f / w;
	float half_y = 0.5f / h;
	return (vec4){
		 col          / w + half_x,    // min_u
		 row          / h + half_y,    // min_v
		(col + 32.0f) / w - half_x,    // max_u
		(row + 32.0f) / h - half_y     // max_v
	};
}


// Loads the embedded default font and builds the shared text material.
// Called by renderer_init() after material2d_init() (we depend on
// material2d_create being available).
void text_init(void) {
	int w, h, ch;
	unsigned char* pixels = stbi_load_from_memory(
		font_atlas_png, font_atlas_png_size, &w, &h, &ch, 4);

	if (!pixels) {
		fprintf(stderr, "Failed to load default font atlas!\n");
		exit(-1);
	}

	font_texture = texture_create_from_memory(
		pixels, (uint32_t)w, (uint32_t)h, 4, DATA_TEXTURE);
	stbi_image_free(pixels);

	if (!font_texture) {
		fprintf(stderr, "Failed to create default font texture!\n");
		exit(-1);
	}

	// One material shared by every glyph submitted via text_draw().
	// BLEND_NORMAL is the default — we keep it explicit here in case the
	// default ever changes underneath us.
	font_material = material2d_create();
	if (!font_material) {
		fprintf(stderr, "Failed to create text material!\n");
		exit(-1);
	}
	material2d_set_diffuse_texture(font_material, font_texture);
	material2d_set_blend_mode(font_material, BLEND_NORMAL);
}


void text_shutdown(void) {
	material2d_destroy(font_material);
	texture_destroy(font_texture);
	font_material = NULL;
	font_texture = NULL;
}


// Backwards-compat accessor — kept because other code (e.g. application or
// debug overlays) may want to use the font texture directly.
texture_t* text_get_default_font_texture(void) {
	return font_texture;
}


/* Submit a text string for rendering through the 2D overlay pipeline.
 * Each character becomes one sprite_submit() call, fed into the same batch
 * used by all other 2D content. The sprites are auto-routed to the UI
 * camera (priority 1000 — drawn on top of everything else).
 *
 * Glyphs use a fixed cell size of 32 logical units, scaled by `size`.
 * The cursor advances by half the scaled cell after each glyph.
 *
 * text   - null-terminated string (length passed separately for non-NUL stops)
 * length - number of characters to render
 * size   - scale factor (1.0 = 32 unit cell)
 * x, y   - top-left corner of the first glyph (UI virtual units)
 * color  - per-glyph tint, multiplied with the atlas texel
 *
 * Note on coordinates: the batch2d quad is centered on the origin, so to
 * place a glyph's top-left corner at (x, y) we translate by (x + s/2, y + s/2)
 * and scale by s. The +s/2 offset is what reconciles "center-pivot mesh"
 * with "top-left position" semantics. */
void text_draw(const char* text, uint32_t length, float size,
               uint32_t x, uint32_t y, color_t color)
{
	float scaled = size * CELL_SIZE;
	float half = scaled * 0.5f;
	float cursor_x = (float)x;
	float cursor_y = (float)y;

	camera2d_t*   ui_cam   = camera2d_get_ui();
	material2d_t* material = font_material;

	for (uint32_t i = 0; i < length; i++) {
		vec4 uv = get_uv_rect(text[i],
			font_texture->width, font_texture->height);

		// translate(cursor + half) × scale(scaled, scaled, 1)
		// — the half offset converts top-left placement to the center-pivot
		// quad used by batch2d.
		mat4 t = mat4_translate(cursor_x + half, cursor_y + half, 0.0f);
		mat4 s = mat4_scale(scaled, scaled, 1.0f);
		mat4 transform = mat4_multiply(t, s);

		sprite_submit(&(sprite_submit_t){
			.camera    = ui_cam,
			.material  = material,
			.transform = transform,
			.uv_rect   = uv,
			.color     = color,
			.layer     = 0,  // text shares the UI camera's layer 0; future
			                 // text_draw_layered() can take a layer argument.
		});

		cursor_x += scaled * GLYPH_ADVANCE;
	}
}
