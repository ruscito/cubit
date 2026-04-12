// shadow.c

#include "shadow.h"
#include "backend.h"
#include "stb_math3d.h"
#include "light.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

static shadow_atlas_t* shadow_atlas;


/* This function update the shadow tile associated to the light */
void shadow_map_update(light_t* light, vec3 *corners, int32_t tile_index) {
    mat4 v, p;

    // Get tile index
    if (tile_index < 0) {
        fprintf(stderr, "shadow_map_update called with invalid shadow tile_index\n");
        return;
    }
    // Extract the tile VP
    mat4 *vp = &shadow_atlas->tiles[tile_index].vp;

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
            *vp = mat4_multiply(p, v);

            // TODO: make sure it works

            // Texel snapping: prevent shadow swimming by rounding
            // the VP translation to the nearest texel boundary
            float tile_sz = (float)shadow_atlas->tiles[tile_index].size;
            float half_sz = tile_sz * 0.5f;

            // Where the origin lands in texel coordinates
            float shadow_x = vp->m[12] * half_sz;
            float shadow_y = vp->m[13] * half_sz;

            // Remove the sub-texel offset
            vp->m[12] += (roundf(shadow_x) - shadow_x) / half_sz;
            vp->m[13] += (roundf(shadow_y) - shadow_y) / half_sz;

            break;

        case LIGHT_SPOT:;
            v = mat4_look_at(light->position, light->direction, (vec3){0.0f, 1.0f, 0.0f});
            p = mat4_perspective(2.0f*light->cone.outer_cutoff, 1.0f, 0.1f, 50.0f);
            *vp = mat4_multiply(p, v);
            break;

        case LIGHT_POINT:
            ;
            break;
        default:
            ;
    }
}

int32_t shadow_get_index_available_tile(void) {
    for(uint32_t i = 0; i < shadow_atlas->tile_count; i++)
        if (!shadow_atlas->tiles[i].occupied) return (int32_t) i;
    return -1;
}

void shadow_set_tile_occupied(uint32_t index, bool status) {
    shadow_atlas->tiles[index].occupied = status;
}


/* The function allocates the atlas structure, clamps the atlas size,
 * clamps the tile size, allocate the array of tiles and travers the
 * array generating the rect. After all of this it call on the
 * backend to allocate the GPU resources for the atlas. */
void shadow_atlas_init(uint32_t atlas_size, uint32_t tile_size) {
    shadow_atlas = malloc(sizeof(*shadow_atlas));
    if (!shadow_atlas) {
		fprintf(stderr, "Failed to allocate memory for the shadow atlas\n");
        exit(-1);
	}

    // TODO: add a chec for even/8 size
    shadow_atlas->atlas_size = atlas_size > DEFAULT_SHADOW_ATLAS_SIZE ?
        DEFAULT_SHADOW_ATLAS_SIZE : atlas_size;
    shadow_atlas->tile_size = tile_size > DEFAULT_SHADOW_TILE_SIZE ?
        DEFAULT_SHADOW_TILE_SIZE : tile_size;
    shadow_atlas->tile_count = (int)(atlas_size / tile_size) * (int)(atlas_size / tile_size);

    // Allocate memory for the tile
    shadow_atlas->tiles = malloc(shadow_atlas->tile_count * sizeof(shadow_tile_t));
    if (!shadow_atlas->tiles) {
		fprintf(stderr, "Failed to allocate memory for the shadow atlas tiles\n");
        exit(-1);
    }

    // +--+--+--+   8192, 8192
    // |  |  |  |
    // +--+--+--+
    // |  |  |  |
    // +--+--+--+
    // |  |  |  |
    // +--+--+--+
    // |  |  |  |
    // +--+--+--+       0,8192

    // Travers and set the tile x, y, size and normalized rect
    uint32_t i = 0;
    uint32_t tiles_per_row = shadow_atlas->atlas_size / shadow_atlas->tile_size;
    uint32_t ts = shadow_atlas->tile_size;
    uint32_t as = shadow_atlas->atlas_size;

    for (uint32_t y = 0; y < tiles_per_row; y++) {
        for (uint32_t x = 0; x < tiles_per_row; x++) {
            shadow_tile_t *tile = &shadow_atlas->tiles[i];
            tile->x = x * ts;
            tile->y = y * ts;
            tile->size = ts;
            tile->rect.x = (float)tile->x / (float)as;
            tile->rect.z = (float)(tile->x + ts) / (float)as;
            tile->rect.y = (float)tile->y / (float)as;
            tile->rect.w = (float)(tile->y + ts) / (float)as;
            tile->occupied = false;
            tile->vp = mat4_identity();
            i++;
        }
    }

    // Now that the shadow atlas structure is defined we can
    // call the backend to generate the resources for the atlas
    shadow_atlas->cascade_count = DEFAULT_CASCADE_COUNT;
    shadow_atlas->lambda = DEFAULT_CASCADE_LAMBDA;
    backend_shadow_atlas_new(shadow_atlas);
}

void shadow_atlas_shutdown(void) {
    backend_shadow_atlas_destroy(shadow_atlas);
    free(shadow_atlas->tiles);
    free(shadow_atlas);
}

shadow_atlas_t* shadow_atlas_get(void) {
    return shadow_atlas;
}

void shadow_compute_split_distances(float near, float far) {
    uint32_t n = shadow_atlas->cascade_count;
    float lambda = shadow_atlas->lambda;

    shadow_atlas->split_distances[0] = near;
    for (uint32_t i = 1; i < n; i++) {
        float p = (float)i / (float)n;
        float log_split = near * powf(far / near, p);
        float lin_split = near + (far - near) * p;
        shadow_atlas->split_distances[i] = lambda * log_split + (1.0f - lambda) * lin_split;
    }
    shadow_atlas->split_distances[n] = far;
}

/* λ = 0 → tagli puramente uniformi
 * λ = 1 → tagli puramente logaritmici
 * λ = 0.5 → via di mezzo, che in pratica funziona benissimo */
void shadow_set_cascade_lambda(float l) {
    shadow_atlas->lambda = l;
}

