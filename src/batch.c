// batch.c

#include "batch.h"
#include "object.h"
#include "material.h"
#include "shader.h"
#include "mesh.h"
#include "stb_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


// Capacity sized to the worst case of MAX_OBJECTS3D = 10000 active objects.
// Opaque and transparent share the cap because at most one of the two paths
// is hot at a time — a scene either is mostly opaque (few transparents) or
// mostly transparent (few opaques).
#define BATCH_MAX_RECORDS 10000


// Pass tag — encoded in the high bits of the sort key so the unsigned sort
// naturally separates the two pass classes.
//
// PASS_OPAQUE = 0 sorts before PASS_TRANSPARENT = 1. Within each pass the
// remaining bits sort by material/mesh (opaque) or by reverse depth
// (transparent).
typedef enum {
	PASS_OPAQUE      = 0,
	PASS_TRANSPARENT = 1,
} batch_pass_t;


// Per-record state captured at push time. Lives in the arena between
// batch_push_object and batch_end_frame.
typedef struct {
	uint64_t           sort_key;
	batch_pass_t       pass;
	mesh_t*            mesh;
	material_t*        material;
	batch_instance_t   instance;
} batch_record_t;


// Module state. Pointers reference arena memory and must not escape this TU.
static struct {
	memory_t arena;

	batch_record_t* records;
	uint32_t        record_count;

	// Final per-pass arrays produced by end_frame.
	batch_group_t*    opaque_groups;
	batch_instance_t* opaque_instances;
	uint32_t          opaque_group_count;
	uint32_t          opaque_instance_count;

	batch_group_t*    transparent_groups;
	batch_instance_t* transparent_instances;
	uint32_t          transparent_group_count;
	uint32_t          transparent_instance_count;

	// Frame-wide camera state.
	mat4 vp;
	vec3 camera_position;
	bool has_camera;
} state;


// ---- Sort key construction ----
//
// Layout (MSB to LSB):
//   [63..62] pass               ( 2 bits)  — 00 = opaque, 01 = transparent
//   [61..48] camera priority    (14 bits)  — biased to unsigned, reserved
//                                            (currently 0; 3D has one camera)
//   [47..32] material id        (16 bits)  — pointer hash, lo 16 bits
//   [31..16] mesh id            (16 bits)  — pointer hash, lo 16 bits
//   [15.. 0] depth bucket       (16 bits)  — for transparents: dist² mapped
//                                            to [0..65535], reversed so far
//                                            objects sort first (drawn first).
//
// Why this exact ordering:
//   - pass at the very top → opaque records are guaranteed contiguous and
//     come before transparent records, regardless of any other field.
//   - For opaque: material > mesh in priority because shader/material
//     switches are more expensive than mesh switches; grouping by material
//     first minimizes program changes and per-material uniform updates.
//   - For transparent: depth_bucket dominates the low half, but pass already
//     dominates the high half — so the depth field becomes the de-facto
//     primary sort within the transparent class.
//
// Pointer hashing: pointers are 8-byte aligned on x86-64; shifting right by 4
// drops alignment zeros and keeps the low entropy. Then we keep 16 bits.
// Two pointers colliding on 16 bits cause a benign mis-grouping (two
// adjacent groups instead of one merged group) — never a correctness bug.
static uint16_t hash_ptr16(const void* p) {
	uintptr_t v = (uintptr_t)p >> 4;
	v ^= v >> 16;
	v ^= v >> 32;
	return (uint16_t)(v & 0xFFFFu);
}


static uint64_t build_sort_key_opaque(material_t* mat, mesh_t* mesh) {
	uint64_t k_pass     = (uint64_t)PASS_OPAQUE & 0x3u;
	uint64_t k_priority = 0; // reserved
	uint64_t k_material = hash_ptr16(mat);
	uint64_t k_mesh     = hash_ptr16(mesh);
	uint64_t k_depth    = 0; // unused for opaque

	return (k_pass     << 62)
	     | (k_priority << 48)
	     | (k_material << 32)
	     | (k_mesh     << 16)
	     |  k_depth;
}


// Transparent: same layout, but pass=1 puts these records at the end of the
// global sort, and depth_bucket carries the back-to-front order. We invert
// the bucket so larger distances sort first — drawn first — meaning far
// objects are drawn first and near objects on top.
static uint64_t build_sort_key_transparent(material_t* mat, mesh_t* mesh, float distance_sq) {
	uint64_t k_pass     = (uint64_t)PASS_TRANSPARENT & 0x3u;
	uint64_t k_priority = 0;
	uint64_t k_material = hash_ptr16(mat);
	uint64_t k_mesh     = hash_ptr16(mesh);

	// Map distance² ∈ [0, ~) onto 16 bits. Cap at 65535. Then invert
	// so far objects (large distance²) get a SMALL bucket value → they sort
	// EARLIER → they get drawn first. Near objects sort later → drawn on top.
	int32_t bucket = (int32_t)distance_sq;
	if (bucket < 0)     bucket = 0;
	if (bucket > 65535) bucket = 65535;
	uint64_t k_depth = (uint64_t)(65535 - bucket) & 0xFFFFu;

	return (k_pass     << 62)
	     | (k_priority << 48)
	     | (k_material << 32)
	     | (k_mesh     << 16)
	     |  k_depth;
}


// ---- Lifecycle ----

// Arena sized for the worst case: every record live, plus per-pass groups
// arrays (one group per record in the absolute worst case), plus per-pass
// instance arrays (same count). Allocated once at init, reset per frame.
static size_t compute_arena_size(void) {
	size_t records  = sizeof(batch_record_t)   * BATCH_MAX_RECORDS;
	size_t groups   = sizeof(batch_group_t)    * BATCH_MAX_RECORDS * 2; // opaque + transparent
	size_t inst     = sizeof(batch_instance_t) * BATCH_MAX_RECORDS * 2;
	size_t slack    = 4096;
	return records + groups + inst + slack;
}


void batch_init(void) {
	memset(&state, 0, sizeof(state));
	arena_create(&state.arena, compute_arena_size());
}


void batch_shutdown(void) {
	arena_destroy(&state.arena);
	memset(&state, 0, sizeof(state));
}


// ---- Frame production ----

void batch_begin_frame(void) {
	arena_reset(&state.arena);

	state.records      = arena_alloc(&state.arena, sizeof(batch_record_t) * BATCH_MAX_RECORDS);
	state.record_count = 0;

	state.opaque_groups            = NULL;
	state.opaque_instances         = NULL;
	state.opaque_group_count       = 0;
	state.opaque_instance_count    = 0;

	state.transparent_groups       = NULL;
	state.transparent_instances    = NULL;
	state.transparent_group_count  = 0;
	state.transparent_instance_count = 0;

	state.has_camera = false;
}


void batch_set_camera(mat4 vp, vec3 camera_position) {
	state.vp              = vp;
	state.camera_position = camera_position;
	state.has_camera      = true;
}


// Bakes object.color × material.surface_color into a single per-instance
// color, mirroring the 2D pipeline. Lit shader needs only one color
// attribute, no per-batch surface_color uniform.
static color_t bake_color(object3d_t* obj) {
	color_t oc = obj->color;
	color_t mc = obj->material->surface_color;
	return (color_t){
		oc.r * mc.r,
		oc.g * mc.g,
		oc.b * mc.b,
		oc.a * mc.a,
	};
}


void batch_push_object(object3d_t* obj) {
	if (!state.has_camera) {
		fprintf(stderr, "batch_push_object: no active camera set this frame\n");
		return;
	}
	if (state.record_count >= BATCH_MAX_RECORDS) {
		fprintf(stderr, "batch_push_object: BATCH_MAX_RECORDS reached\n");
		return;
	}

	batch_record_t* rec = &state.records[state.record_count++];
	rec->mesh     = obj->mesh;
	rec->material = obj->material;

	rec->instance.transform = obj->transform;
	rec->instance.uv_rect   = obj->uv_rect;
	rec->instance.color     = bake_color(obj);

	if (obj->material->opacity < 1.0f) {
		rec->pass = PASS_TRANSPARENT;
		float dx = state.camera_position.x - obj->transform.m[12];
		float dy = state.camera_position.y - obj->transform.m[13];
		float dz = state.camera_position.z - obj->transform.m[14];
		float d2 = dx*dx + dy*dy + dz*dz;
		rec->sort_key = build_sort_key_transparent(obj->material, obj->mesh, d2);
	} else {
		rec->pass = PASS_OPAQUE;
		rec->sort_key = build_sort_key_opaque(obj->material, obj->mesh);
	}
}


// ---- End-of-frame: sort, build groups, emit instances ----

static int compare_records(const void* a, const void* b) {
	uint64_t ka = ((const batch_record_t*)a)->sort_key;
	uint64_t kb = ((const batch_record_t*)b)->sort_key;
	if (ka < kb) return -1;
	if (ka > kb) return  1;
	return 0;
}


// Walks the (already-sorted) records, splits them into opaque and transparent
// passes, builds the group arrays for each pass, and copies instance data
// into per-pass contiguous arrays.
//
// Within a pass, a group break happens when (mesh, material) changes. Opaque
// groups instance multiple objects sharing the same mesh+material. Transparent
// groups end up mostly count=1 because depth varies per object — that's
// expected and unchanged from the pre-Stage-23 behavior.
static void build_groups(void) {
	if (state.record_count == 0) return;

	// Allocate worst-case-sized output arrays. We don't know the split ahead
	// of time, but the total is bounded by record_count and we have BATCH_MAX_RECORDS
	// slack on each pass.
	state.opaque_groups        = arena_alloc(&state.arena, sizeof(batch_group_t) * BATCH_MAX_RECORDS);
	state.opaque_instances     = arena_alloc(&state.arena, sizeof(batch_instance_t) * BATCH_MAX_RECORDS);
	state.transparent_groups   = arena_alloc(&state.arena, sizeof(batch_group_t) * BATCH_MAX_RECORDS);
	state.transparent_instances= arena_alloc(&state.arena, sizeof(batch_instance_t) * BATCH_MAX_RECORDS);

	uint32_t op_inst = 0, op_grp = 0;
	uint32_t tr_inst = 0, tr_grp = 0;

	mesh_t*     prev_mesh = NULL;
	material_t* prev_mat  = NULL;
	batch_pass_t prev_pass = (batch_pass_t)-1;

	for (uint32_t i = 0; i < state.record_count; i++) {
		const batch_record_t* rec = &state.records[i];

		bool pass_change = (rec->pass != prev_pass);
		bool group_break = pass_change ||
		                   rec->mesh     != prev_mesh ||
		                   rec->material != prev_mat;

		if (rec->pass == PASS_OPAQUE) {
			if (group_break) {
				state.opaque_groups[op_grp++] = (batch_group_t){
					.mesh            = rec->mesh,
					.material        = rec->material,
					.vp              = state.vp,
					.instance_offset = op_inst,
					.instance_count  = 0,
				};
			}
			state.opaque_instances[op_inst++] = rec->instance;
			state.opaque_groups[op_grp - 1].instance_count++;
		} else {
			if (group_break) {
				state.transparent_groups[tr_grp++] = (batch_group_t){
					.mesh            = rec->mesh,
					.material        = rec->material,
					.vp              = state.vp,
					.instance_offset = tr_inst,
					.instance_count  = 0,
				};
			}
			state.transparent_instances[tr_inst++] = rec->instance;
			state.transparent_groups[tr_grp - 1].instance_count++;
		}

		prev_mesh = rec->mesh;
		prev_mat  = rec->material;
		prev_pass = rec->pass;
	}

	state.opaque_group_count        = op_grp;
	state.opaque_instance_count     = op_inst;
	state.transparent_group_count   = tr_grp;
	state.transparent_instance_count= tr_inst;
}


void batch_end_frame(void) {
	if (state.record_count == 0) return;

	qsort(state.records, state.record_count, sizeof(batch_record_t), compare_records);
	build_groups();
}


// ---- Consumption API ----

const batch_group_t* batch_get_opaque_groups(void) {
	return state.opaque_groups;
}

uint32_t batch_get_opaque_group_count(void) {
	return state.opaque_group_count;
}

const batch_group_t* batch_get_transparent_groups(void) {
	return state.transparent_groups;
}

uint32_t batch_get_transparent_group_count(void) {
	return state.transparent_group_count;
}

const batch_instance_t* batch_get_opaque_instances(void) {
	return state.opaque_instances;
}

const batch_instance_t* batch_get_transparent_instances(void) {
	return state.transparent_instances;
}
