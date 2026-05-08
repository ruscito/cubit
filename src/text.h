// text.h

#ifndef TEXT_H_
#define TEXT_H_

#include "cubit_types.h"


// Engine-internal lifecycle (called by renderer_init/shutdown).
void text_init(void);
void text_shutdown(void);

// Public-ish: expose the font texture for game code that wants to use the
// default font on its own materials. Stable contract; left here even though
// stage 22 step 9 made the rest of the old text-pass API obsolete.
texture_t* text_get_default_font_texture(void);


#endif // TEXT_H_
