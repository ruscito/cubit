// text.h

#ifndef TEXT_H_
#define TEXT_H_

#include "cubit_types.h"

// Internal API for the backend
void        text_init(void);
void        text_shutdown(void);
mat4*       text_get_transforms(void);
vec4*       text_get_uv_rects(void);
uint32_t    text_get_count(void);
mesh_t*     text_get_quad(void);
color_t*    text_get_colors(void);
texture_t*  text_get_texture(void);
void        text_reset(void);


#endif // TEXT_H_


