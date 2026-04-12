// light.c

#include "light.h"
#include "shadow.h"

#include <stdio.h>
#include <string.h>

static struct {
	light_t lights[MAX_LIGHTS];
	uint32_t count;
} light_table;


static void reset_light(light_t* l) {
	l->type = LIGHT_OFF;
	l->color = COLOR_WHITE;
	l->intensity = DEFAULT_INTENSITY;
	l->direction = (vec3){0.0f, -1.0f, 0.0f};
	l->position = (vec3){0.0f, 0.0f, 0.0f};
	l->attenuation.constant = DEFAULT_ATT_CONSTANT;
	l->attenuation.linear = DEFAULT_ATT_LINEAR;
	l->attenuation.quadratic = DEFAULT_ATT_QUADRATIC;
	l->cone.inner_cutoff = DEFAULT_CONE_INNER * DEG2RAD;
	l->cone.outer_cutoff = DEFAULT_CONE_OUTER * DEG2RAD;
    l->cascade_count = 0;
    for (uint32_t i = 0; i < MAX_CASCADES; i++) l->cascade_tiles[i] = -1;
}


void light_init(void) {
	light_table.count = 0;
	for (uint32_t i = 0; i < MAX_LIGHTS; i++) {
		reset_light(&light_table.lights[i]);
	}
}

void light_shutdown(void) {
	light_table.count = 0;
}
light_t* light_get_table(void) {
	return light_table.lights;
}

uint32_t light_get_count(void) {
	return light_table.count;
}

int32_t light_create(LightTypes type) {
	if (light_table.count >= MAX_LIGHTS) {
		fprintf(stderr, "Light table full, cannot create light\n");
		return -1;
	}
	uint32_t index = light_table.count;
	reset_light(&light_table.lights[index]);
	light_table.lights[index].type = type;
	light_table.count++;
	return (int32_t)index;
}

void light_destroy(int32_t index) {
	if (index < 0 || index >= (int32_t)light_table.count) return;

	// Shift remaining lights down to keep the table compact
	for (uint32_t i = (uint32_t)index; i < light_table.count - 1; i++) {
		light_table.lights[i] = light_table.lights[i + 1];
	}
	reset_light(&light_table.lights[light_table.count - 1]);
	light_table.count--;
}

void light_set_color(int32_t index, color_t color) {
	if (index < 0 || index >= (int32_t)light_table.count) return;
	light_table.lights[index].color = color;
}

void light_set_intensity(int32_t index, float intensity) {
	if (index < 0 || index >= (int32_t)light_table.count) return;
	light_table.lights[index].intensity = intensity;
}

void light_set_direction(int32_t index, vec3 direction) {
	if (index < 0 || index >= (int32_t)light_table.count) return;
	light_table.lights[index].direction = vec3_normalize(direction);
}

void light_set_position(int32_t index, vec3 position) {
	if (index < 0 || index >= (int32_t)light_table.count) return;
	light_table.lights[index].position = position;
}

void light_set_attenuation(int32_t index, float constant, float linear, float quadratic) {
	if (index < 0 || index >= (int32_t)light_table.count) return;
	light_table.lights[index].attenuation.constant = constant;
	light_table.lights[index].attenuation.linear = linear;
	light_table.lights[index].attenuation.quadratic = quadratic;
}

void light_set_cone(int32_t index, float inner_degrees, float outer_degrees) {
	if (index < 0 || index >= (int32_t)light_table.count) return;
	light_table.lights[index].cone.inner_cutoff = inner_degrees * DEG2RAD;
	light_table.lights[index].cone.outer_cutoff = outer_degrees * DEG2RAD;
}

/* Get the index of the light in the light table, travers the
 * shadow tiles atlas, find the first available assigne the
 * position to the light and mark the tile as occupied */
void light_enable_shadow(int32_t index) {
    if (index < 0 || index >= (int32_t)light_table.count) return;
    light_t *l = &light_table.lights[index];

    int32_t count = (l->type == LIGHT_DIRECTIONAL) ? MAX_CASCADES : 1;

    for (int32_t i = 0; i < count; i++) {
        int32_t pos = shadow_get_index_available_tile();
        if (pos < 0) {
            // Not enough tiles — release what we already grabbed
            for (int32_t j = 0; j < i; j++) {
                shadow_set_tile_occupied(l->cascade_tiles[j], false);
                l->cascade_tiles[j] = -1;
            }
            l->cascade_count = 0;
            fprintf(stderr, "light_enable_shadow: not enough atlas tiles\n");
            return;
        }
        l->cascade_tiles[i] = pos;
        shadow_set_tile_occupied(pos, true);
    }
    l->cascade_count = count;
}

void light_disable_shadow(int32_t index) {
    if (index < 0 || index >= (int32_t)light_table.count) return;
    light_t *l = &light_table.lights[index];

    for (int32_t i = 0; i < l->cascade_count; i++) {
        if (l->cascade_tiles[i] >= 0) {
            shadow_set_tile_occupied(l->cascade_tiles[i], false);
            l->cascade_tiles[i] = -1;
        }
    }
    l->cascade_count = 0;
}
