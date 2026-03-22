// shadow.c

#include "shadow.h"
#include "backend.h"
#include "stb_math3d.h"
#include "light.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>


shadow_map_t *shadow_map_create(void) {
	shadow_map_t *sm = malloc(sizeof(*sm));
	if (!sm) {
		fprintf(stderr, "Failed to allocate memory for the shadow map\n");
		return NULL;
	}

	sm->size = DEFAULT_SM_SIZE;
	sm->vp = mat4_identity();
	backend_shadow_map_new(sm);

	return sm;
}


void shadow_map_destroy(shadow_map_t* s) {
	backend_shadow_map_destroy(s);
	free(s);
}


/* This function update the light view matrix associated to the shadow
 * map passed. The light view matrix is a look at the scene from
 * the light point of view (in light coordinates) */
void shadow_map_update(shadow_map_t* s, light_t* light, vec3 *corners) {
    mat4 v, p;
    switch (light->type) {
        case LIGHT_DIRECTIONAL:;
            // Get the 8 corners
            vec3 c[8];
            memcpy((void *)c, (const void*) corners, 8 * sizeof(vec3));

            // Calculate the corners' center
            vec3 center = {0.0f, 0.0f, 0.0f};
            for (uint32_t i = 0; i < 8; i++) {
                center = vec3_add(center, corners[i]);
            }
            center = vec3_scale(center, 0.125f);

            // Position the light from the center
            // along the invers of the light direction
            vec3 pos = vec3_add(center, vec3_scale(light->direction, -DEFAULT_SM_DISTANCE));

            // Build light view matrix
            v = mat4_look_at(pos, light->direction, (vec3){0.0f, 1.0f, 0.0f});

	        float min_x = FLT_MAX;
            float max_x = -FLT_MAX;
            float min_y = FLT_MAX;
            float max_y = -FLT_MAX;
            float min_z = FLT_MAX;
            float max_z = -FLT_MAX;

            // Trnslate 8 corners in light space
            for (uint32_t i = 0; i < 8; i++) {
                c[i] = vec3_multiply_mat4(c[i], v);
                if (c[i].x >= max_x) max_x = c[i].x;
                if (c[i].x <= min_x) min_x = c[i].x;
                if (c[i].y >= max_y) max_y = c[i].y;
                if (c[i].y <= min_y) min_y = c[i].y;
                if (c[i].z >= max_z) max_z = c[i].z;
                if (c[i].z <= min_z) min_z = c[i].z;
            }

            // Get the prospective
            p = mat4_orthographic(min_x, max_x, min_y, max_y, -max_z, -min_z);
            s->vp = mat4_multiply(p, v);
            break;

        case LIGHT_SPOT:;
            v = mat4_look_at(light->position, light->direction, (vec3){0.0f, 1.0f, 0.0f});
            p = mat4_perspective(2.0f*light->cone.outer_cutoff, 1.0f, 0.1f, 50.0f);
            s->vp = mat4_multiply(p, v);
            break;

        case LIGHT_POINT:
            ;
            break;
        default:
            ;
    }
}

