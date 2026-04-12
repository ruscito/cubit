// collision.c

#include "collision.h"
#include "object.h"
#include "cubit.h"

#include <stdbool.h>
#include <stdlib.h>
#include <float.h>


//TODO: octre or grid in case of more items
#define MAX_3D_OBJ 1000

static object3d_t* obj3d[MAX_3D_OBJ];
static uint32_t obj3d_count;


static void collision_3d_reset(void) {
    obj3d_count = 0;
}

void collision_init(void) {
    collision_3d_reset();
}

/* Nothing to do for now */
void collision_shutdown(void) {
    ;
}

bool collision_3d_test(object3d_t* a, object3d_t* b) {
    aabb_t abox = a->aabb;
    aabb_t bbox = b->aabb;
    // TODO: add bitwised check
    return (abox.min_x < bbox.max_x) && (abox.max_x > bbox.min_x) &&
        (abox.min_y < bbox.max_y) && (abox.max_y > bbox.min_y) &&
        (abox.min_z < bbox.max_z) && (abox.max_z > bbox.min_z);
}

object3d_t* collision_3d_test_all(object3d_t* a, uint32_t* iter) {
    if (*iter >= obj3d_count) return NULL;
    for (uint32_t i = *iter; i < obj3d_count; i++) {
        if (collision_3d_test(a, obj3d[i]) && a != obj3d[i]) {
                *iter = i + 1;
                return obj3d[i];
        }
    }
    return NULL;
}

void collision_3d_add_collidable(object3d_t* a){
    for (uint32_t i = 0; i < obj3d_count; i++) {
        if (obj3d[i] == a) return;
    }
    obj3d[obj3d_count] = a;
    obj3d_count ++;
}


void collision_3d_remove_collidable(object3d_t* a) {
    for (uint32_t i = 0; i < obj3d_count; i++) {
        if (obj3d[i] == a) {
            obj3d[i] = obj3d[obj3d_count - 1];
            obj3d_count--;
            return;
        }
    }
}

/* per ogni asse calcola quanto A penetra dentro B da entrambi i lati,
 * sceglie il lato con meno sovrapposizione (quello da cui è entrato),
 * poi tra i tre assi prende quello con lo spostamento più piccolo e
 * applica solo quello. Il dirty flag alla fine garantisce che
 * l'AABB venga ricalcolato al prossimo frame.*/
void collision_3d_resolve_slide(object3d_t* a, object3d_t* b) {
    aabb_t abox = object3d_get_aabb(a);
    aabb_t bbox = object3d_get_aabb(b);

    // penetrazione su ogni asse (da entrambe le direzioni)
    float pen_right = abox.max_x - bbox.min_x;  // A entra da sinistra di B
    float pen_left  = bbox.max_x - abox.min_x;  // A entra da destra di B
    float pen_up    = abox.max_y - bbox.min_y;
    float pen_down  = bbox.max_y - abox.min_y;
    float pen_front = abox.max_z - bbox.min_z;
    float pen_back  = bbox.max_z - abox.min_z;

    // per ogni asse, prendi la penetrazione minore e la sua direzione
    float resolve_x = (pen_right < pen_left) ? -pen_right : pen_left;
    float resolve_y = (pen_up < pen_down)    ? -pen_up    : pen_down;
    float resolve_z = (pen_front < pen_back) ? -pen_front : pen_back;

    // trova l'asse con la penetrazione minima in valore assoluto
    float abs_x = (resolve_x < 0) ? -resolve_x : resolve_x;
    float abs_y = (resolve_y < 0) ? -resolve_y : resolve_y;
    float abs_z = (resolve_z < 0) ? -resolve_z : resolve_z;

    if (abs_x <= abs_y && abs_x <= abs_z) {
        a->position.x += resolve_x;
    } else if (abs_y <= abs_z) {
        a->position.y += resolve_y;
    } else {
        a->position.z += resolve_z;
    }

    object3d_set_position(a, a->position);
}


raycast_3d_result_t collision_3d_raycast(vec3 origin, vec3 direction, float max_distance) {
    raycast_3d_result_t result = { NULL, {0,0,0}, max_distance, false };

    for (uint32_t i = 0; i < obj3d_count; i++) {
        aabb_t box = object3d_get_aabb(obj3d[i]);

        float t_min = -FLT_MAX;
        float t_max =  FLT_MAX;

        // X slab
        if (direction.x != 0.0f) {
            float t1 = (box.min_x - origin.x) / direction.x;
            float t2 = (box.max_x - origin.x) / direction.x;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > t_min) t_min = t1;
            if (t2 < t_max) t_max = t2;
        } else if (origin.x < box.min_x || origin.x > box.max_x) {
            continue;
        }

        // Y slab
        if (direction.y != 0.0f) {
            float t1 = (box.min_y - origin.y) / direction.y;
            float t2 = (box.max_y - origin.y) / direction.y;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > t_min) t_min = t1;
            if (t2 < t_max) t_max = t2;
        } else if (origin.y < box.min_y || origin.y > box.max_y) {
            continue;
        }

        // Z slab
        if (direction.z != 0.0f) {
            float t1 = (box.min_z - origin.z) / direction.z;
            float t2 = (box.max_z - origin.z) / direction.z;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > t_min) t_min = t1;
            if (t2 < t_max) t_max = t2;
        } else if (origin.z < box.min_z || origin.z > box.max_z) {
            continue;
        }

        if (t_min > t_max) continue;    // mancato
        if (t_max < 0.0f) continue;     // box dietro l'origine

        float t = (t_min >= 0.0f) ? t_min : t_max;
        if (t > max_distance) continue;  // troppo lontano

        if (t < result.distance) {
            result.object = obj3d[i];
            result.distance = t;
            result.point = (vec3){
                origin.x + direction.x * t,
                origin.y + direction.y * t,
                origin.z + direction.z * t
            };
            result.hit = true;
        }
    }

    return result;
}

