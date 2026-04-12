// shadow.h

#ifndef SHADOW_H_
#define SHADOW_H_

#include "cubit_types.h"

#define DEFAULT_SHADOW_ATLAS_SIZE   8192
#define DEFAULT_SHADOW_TILE_SIZE    2048
#define DEFAULT_SHADOW_TILES        64      // Default max nuber of shadow tails
#define DEFAULT_SM_DISTANCE         20      // TODO:maybe to be removed
#define DEFAULT_CASCADE_COUNT       3
#define DEFAULT_CASCADE_LAMBDA      0.5f


/* A tile is a region inside the shadow atlas.
 * Each shadow-casting light gets one tile to render
 * its depth into. The tile knows where it sits in the
 * atlas (pixel coords for glViewport) and its normalized
 * rect (for the fragment shader to sample the right area) */
struct shadow_tile_t{
	uint32_t x;         // pixel position in atlas (left)
	uint32_t y;         // pixel position in atlas (bottom)
	uint32_t size;      // tile width/height in pixels
	vec4 rect;          // normalized coords (u_min, v_min, u_max, v_max)
	mat4 vp;            // light view-projection for this tile
	bool occupied;      // true if a light owns this tile
} ;


/* The atlas owns the single FBO and depth texture
 * that all shadow maps share. Created once at renderer
 * init, destroyed at shutdown */
struct shadow_atlas_t {
	uint32_t fbo;           // single framebuffer
	uint32_t texture_id;    // single depth texture
	uint32_t atlas_size;    // total atlas size in pixels (e.g. 8192)
	uint32_t tile_size;     // per-tile size in pixels (e.g. 2048)
	uint32_t tile_count;    // total tiles: (atlas_size/tile_size)^2
	shadow_tile_t *tiles;   // the array of tiles
    uint32_t cascade_count; // how many cascate have been used (non directional
                            // light will use only one cascade)
    float    lambda;        // used to calculate the cascate split distance
    float    split_distances[MAX_CASCADES + 1]; // N+1 boundaries (near ... far)
} ;


// Atlas lifecycle (called by renderer)
void shadow_atlas_init(uint32_t atlas_size, uint32_t tile_size);
void shadow_atlas_shutdown(void);
shadow_atlas_t* shadow_atlas_get(void);
int32_t shadow_get_index_available_tile(void);
void shadow_set_tile_occupied(uint32_t index, bool status);
void shadow_map_update(light_t* light, vec3 *corners, int32_t tile_index);
void shadow_compute_split_distances(float near, float far);

#endif
