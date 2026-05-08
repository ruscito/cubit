// material2d.h

#ifndef MATERIAL2D_H_
#define MATERIAL2D_H_

#include "cubit_types.h"


struct material2d_t {
	color_t color;              // global tint (multiplied with texture and instance color)
	texture_t* diffuse_texture; // sprite atlas or single image; fallback to 1x1 white
	shader_t* shader;           // sprite shader (set in stage 22 step 4)
	blend_mode_t blend_mode;    // how the sprite blends with what's behind
};


void material2d_init(void);
void material2d_shutdown(void);
material2d_t* material2d_get_default(void);

material2d_t* material2d_create(void);
void material2d_destroy(material2d_t* m);

void material2d_set_color(material2d_t* m, color_t c);
void material2d_set_diffuse_texture(material2d_t* m, texture_t* t);
void material2d_set_shader(material2d_t* m, shader_t* s);
void material2d_set_blend_mode(material2d_t* m, blend_mode_t mode);


#endif
