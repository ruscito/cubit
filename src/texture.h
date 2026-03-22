// texture.h

#ifndef TEXTURE_H_
#define TEXTURE_H_

#include "cubit_types.h"

#define NORMAL 0
#define DIFFUSE 1

struct texture_t {
	uint32_t id;
	uint32_t width;
	uint32_t height;
	int32_t channels;
	FilteringMode min_filter;
	FilteringMode mag_filter;
	WrappingMode s_wrap;
	WrappingMode t_wrap;
};


void texture_init(void);
void texture_shutdown(void);
texture_t* texture_get_default(uint32_t t);


#endif
