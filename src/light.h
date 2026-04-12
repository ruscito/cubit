// light.h

#ifndef LIGHT_H_
#define LIGHT_H_

#include "cubit_types.h"


#define MAX_LIGHTS 16
#define DEFAULT_INTENSITY		1.0f
#define DEFAULT_ATT_CONSTANT	1.0f
#define DEFAULT_ATT_LINEAR		0.09f
#define DEFAULT_ATT_QUADRATIC	0.032f
#define DEFAULT_CONE_INNER		12.5f
#define DEFAULT_CONE_OUTER		17.5f


struct light_t {
	LightTypes type;
	color_t color;
	float intensity;
	vec3 direction;
	vec3 position;
	struct {
		float constant;
		float linear;
		float quadratic;
	} attenuation;
	struct {
		float inner_cutoff;
		float outer_cutoff;
	} cone;
    int32_t cascade_tiles[MAX_CASCADES];// -1 no tile assigned
    int32_t cascade_count;              // 1 for spot/point light MAX_CASCADES
                                        // for directional lights CSM
};

void light_init(void);
void light_shutdown(void);
light_t* light_get_table(void);
uint32_t light_get_count(void);

#endif
