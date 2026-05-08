// batch2d.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "glad/glad.h"

#include "batch2d.h"
#include "object2d.h"
#include "camera2d.h"
#include "material2d.h"
#include "texture.h"
#include "stb_memory.h"


// Maximum number of sprites the batch can accept in a single frame.
// Sized to match the object2d pool so that a worst-case frame (every retained
// object active, plus a margin for immediate sprites) fits without reallocation.
// Increase if the future immediate-sprite traffic (text, particles, debug)
// pushes us close to the cap.
#define BATCH2D_MAX_SPRITES (MAX_OBJECTS2D * 2)


// Internal per-sprite record kept in the arena while we collect the frame.
// Carries the GPU instance data plus the sort key used to order draws.
//
// AoS choice (Array of Structs) is deliberate: qsort on this array moves the
// instance data into final draw order in one pass, so the per-attribute
// upload at end-of-frame becomes three linear loops with no random-access
// gather. Profiles consistently show this beats SoA + gather for our sprite
// counts (~10k–20k per frame).
//
// The sort key packs (camera_priority, layer, blend_mode, texture_id,
// submission_index) into a single uint64. With every sprite getting a unique
// submission_index, no two records can ever tie — that lets us use a
// non-stable sort (qsort) and still get deterministic, submission-preserving
// ordering inside any group.
//
// Bit layout of sort_key (most significant first):
//   [16 bits priority][16 bits layer][4 bits blend][12 bits texture][16 bits submission]
// Cast notes:
//   priority is signed in camera2d_t but we shift it into the unsigned range
//   so that "higher priority = larger key = drawn last" works naturally.
typedef struct {
	uint64_t            sort_key;
	camera2d_t*         camera;
	blend_mode_t        blend_mode;
	uint32_t            texture_id;
	batch2d_instance_t  instance;
} batch2d_record_t;


// Module state. All pointers refer to memory we own (arena or GPU buffers)
// and must not escape this translation unit.
static struct {
	memory_t arena;                 // per-frame scratch memory (records, instances, groups)

	batch2d_record_t*   records;    // arena-allocated, BATCH2D_MAX_SPRITES capacity
	uint32_t            record_count;

	batch2d_instance_t* instances;  // arena-allocated, contiguous GPU upload buffer
	uint32_t            instance_count;

	batch2d_group_t*    groups;     // arena-allocated, built in end_frame()
	uint32_t            group_count;

	uint32_t submission_counter;    // monotonic per-frame, reset in begin_frame

	// Shared sprite quad — centered on origin, vertices in (-0.5, 0.5).
	// The instanced VBOs for model/uv_rect/color are attributes of this VAO.
	GLuint quad_vao;
	GLuint quad_vbo_pos;            // static: 4 vec2 vertices
	GLuint quad_vbo_uv;             // static: 4 vec2 UVs (0..1)
	GLuint quad_ebo;                // static: 6 indices
	GLuint instance_vbo_model;      // dynamic: BATCH2D_MAX_SPRITES * mat4
	GLuint instance_vbo_uv_rect;    // dynamic: BATCH2D_MAX_SPRITES * vec4
	GLuint instance_vbo_color;      // dynamic: BATCH2D_MAX_SPRITES * vec4
} state;


// Static quad geometry. Centered on origin so that object2d_t.position
// translates the sprite center directly — no per-sprite offset needed.
// Z is implicit (zero) because we use vec2 attributes; layering is done
// in software via sort order, not via depth.
static const float QUAD_POS[] = {
	-0.5f, -0.5f,
	 0.5f, -0.5f,
	 0.5f,  0.5f,
	-0.5f,  0.5f,
};

static const float QUAD_UV[] = {
	0.0f, 0.0f,
	1.0f, 0.0f,
	1.0f, 1.0f,
	0.0f, 1.0f,
};

static const uint32_t QUAD_INDICES[] = {
	0, 1, 2,
	2, 3, 0,
};


// Builds the sprite quad VAO and the three instanced VBOs.
// Called once from batch2d_init() — never re-entered, the VAO lives until
// shutdown.
//
// Attribute layout matches the 2D shader (set up at stage 22 step 4):
//   loc 0: vec2 position (per-vertex, static)
//   loc 2: vec2 uv       (per-vertex, static)
//   loc 6..9: mat4 model (per-instance, streamed per frame)
//   loc 10: vec4 uv_rect (per-instance, streamed per frame)
//   loc 11: vec4 color   (per-instance, streamed per frame)
static void create_quad_vao(void) {
	glGenVertexArrays(1, &state.quad_vao);
	glBindVertexArray(state.quad_vao);

	// --- static per-vertex attributes ---

	// Position (loc 0)
	glGenBuffers(1, &state.quad_vbo_pos);
	glBindBuffer(GL_ARRAY_BUFFER, state.quad_vbo_pos);
	glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_POS), QUAD_POS, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glEnableVertexAttribArray(0);

	// UV (loc 2)
	glGenBuffers(1, &state.quad_vbo_uv);
	glBindBuffer(GL_ARRAY_BUFFER, state.quad_vbo_uv);
	glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_UV), QUAD_UV, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glEnableVertexAttribArray(2);

	// Index buffer
	glGenBuffers(1, &state.quad_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.quad_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES), QUAD_INDICES, GL_STATIC_DRAW);

	// --- dynamic per-instance attributes ---

	// Model matrix occupies four consecutive vec4 attribute slots (6..9).
	// Each glVertexAttribDivisor(loc, 1) marks the slot as advancing once per
	// instance instead of once per vertex.
	glGenBuffers(1, &state.instance_vbo_model);
	glBindBuffer(GL_ARRAY_BUFFER, state.instance_vbo_model);
	glBufferData(GL_ARRAY_BUFFER, sizeof(mat4) * BATCH2D_MAX_SPRITES, NULL, GL_DYNAMIC_DRAW);
	for (int i = 0; i < 4; i++) {
		glVertexAttribPointer(6 + i, 4, GL_FLOAT, GL_FALSE,
			sizeof(mat4), (void*)(sizeof(float) * 4 * i));
		glEnableVertexAttribArray(6 + i);
		glVertexAttribDivisor(6 + i, 1);
	}

	// uv_rect (loc 10)
	glGenBuffers(1, &state.instance_vbo_uv_rect);
	glBindBuffer(GL_ARRAY_BUFFER, state.instance_vbo_uv_rect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec4) * BATCH2D_MAX_SPRITES, NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), (void*)0);
	glEnableVertexAttribArray(10);
	glVertexAttribDivisor(10, 1);

	// color (loc 11)
	glGenBuffers(1, &state.instance_vbo_color);
	glBindBuffer(GL_ARRAY_BUFFER, state.instance_vbo_color);
	glBufferData(GL_ARRAY_BUFFER, sizeof(color_t) * BATCH2D_MAX_SPRITES, NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(color_t), (void*)0);
	glEnableVertexAttribArray(11);
	glVertexAttribDivisor(11, 1);

	glBindVertexArray(0);
}


// Tears down the quad VAO and all its attached buffers.
// Called from batch2d_shutdown(). Order does not matter — driver cleans up
// references when the VAO is deleted, but explicit deletion is clearer.
static void destroy_quad_vao(void) {
	glDeleteBuffers(1, &state.instance_vbo_color);
	glDeleteBuffers(1, &state.instance_vbo_uv_rect);
	glDeleteBuffers(1, &state.instance_vbo_model);
	glDeleteBuffers(1, &state.quad_ebo);
	glDeleteBuffers(1, &state.quad_vbo_uv);
	glDeleteBuffers(1, &state.quad_vbo_pos);
	glDeleteVertexArrays(1, &state.quad_vao);
}


// Sizes the per-frame arena to comfortably hold the worst case:
//   - record array (AoS, used during collect + sort)
//   - group array (one group per sprite in the absolute worst case)
//   - three SoA instance arrays (model, uv_rect, color) allocated in
//     end_frame for the GPU upload — these are the destination buffers
//     after the post-sort extraction pass
// Plus alignment slack. Allocated once, reset every frame.
//
// Important: instances must NOT be allocated in begin_frame. The actual
// per-sprite GPU data lives inside the AoS record; the SoA dst arrays in
// end_frame are the only "instance arrays" we need. Double-counting them
// here would not be a correctness bug, but allocating them twice (once in
// begin and once in end) overflowed the arena and returned NULL — that was
// the cause of the early crash in build_groups_and_extract.
static size_t compute_arena_size(void) {
	size_t records   = sizeof(batch2d_record_t)   * BATCH2D_MAX_SPRITES;
	size_t groups    = sizeof(batch2d_group_t)    * BATCH2D_MAX_SPRITES;
	size_t dst_model    = sizeof(mat4)    * BATCH2D_MAX_SPRITES;
	size_t dst_uv_rect  = sizeof(vec4)    * BATCH2D_MAX_SPRITES;
	size_t dst_color    = sizeof(color_t) * BATCH2D_MAX_SPRITES;
	size_t slack     = 4096; // alignment padding margin
	return records + groups + dst_model + dst_uv_rect + dst_color + slack;
}


void batch2d_init(void) {
	memset(&state, 0, sizeof(state));

	arena_create(&state.arena, compute_arena_size());

	create_quad_vao();
}


void batch2d_shutdown(void) {
	destroy_quad_vao();
	arena_destroy(&state.arena);
	memset(&state, 0, sizeof(state));
}


// ---- Sort key construction ----
//
// Builds a 64-bit key whose unsigned numeric order matches the desired
// rendering order: lower value = drawn first, higher value = drawn last
// (and therefore visually on top).
//
// Bit layout (MSB to LSB):
//   [63..48] camera priority (16 bits, biased to unsigned)
//   [47..32] layer            (16 bits, biased to unsigned)
//   [31..28] blend mode       ( 4 bits, enum index)
//   [27..16] texture id       (12 bits, low bits of GL handle)
//   [15.. 0] submission index (16 bits, monotonic per-frame counter)
//
// Why the bias on priority/layer:
//   These are signed int32 in the public API but we only have 16 bits of room
//   each in the key. We clamp to int16 range and then add 0x8000 so that
//   INT16_MIN..INT16_MAX maps cleanly onto 0..0xFFFF in the unsigned key.
//   Clamping is safe in practice — nobody assigns layer = 70000 — but we still
//   guard against accidental overflow.
//
// Why texture_id only takes 12 bits:
//   GL texture names are uint32 but in practice live in a small range. 12 bits
//   = 4096 distinct ids per frame is more than enough for grouping purposes.
//   Two textures with colliding low bits will end up adjacent in the sort,
//   which means they get bound back-to-back — slight inefficiency, never
//   incorrect.
//
// Why submission_index is the LSB:
//   It is the final tiebreaker. As long as every sprite gets a unique
//   submission_index, no two records can share a sort_key, which lets us use
//   a non-stable qsort and still get deterministic, submission-preserving
//   ordering inside any group.
static uint64_t build_sort_key(int32_t camera_priority,
                               int32_t layer,
                               blend_mode_t blend,
                               uint32_t texture_id,
                               uint32_t submission_index)
{
	// Clamp to int16 range, then bias to unsigned. The +0x8000 is what makes
	// negative values sort before positive values in unsigned comparison.
	if (camera_priority >  32767) camera_priority =  32767;
	if (camera_priority < -32768) camera_priority = -32768;
	if (layer            >  32767) layer            =  32767;
	if (layer            < -32768) layer            = -32768;

	uint64_t k_priority = (uint64_t)((uint16_t)(camera_priority + 0x8000));
	uint64_t k_layer    = (uint64_t)((uint16_t)(layer            + 0x8000));
	uint64_t k_blend    = (uint64_t)blend         & 0xFu;     // 4 bits
	uint64_t k_texture  = (uint64_t)texture_id    & 0xFFFu;   // 12 bits
	uint64_t k_submit   = (uint64_t)submission_index & 0xFFFFu;

	return (k_priority << 48)
	     | (k_layer    << 32)
	     | (k_blend    << 28)
	     | (k_texture  << 16)
	     |  k_submit;
}


// Resets all per-frame state. The arena is the single source of truth for
// per-frame memory, so a single arena_reset() rewinds every transient buffer
// at once.
//
// After this returns:
//   - records and groups point at empty (but allocated) arena storage,
//     safe to write to up to BATCH2D_MAX_SPRITES entries.
//   - The three SoA dst arrays (model/uv_rect/color) used by the GPU upload
//     are NOT allocated here — they are allocated in end_frame() right
//     before the extraction pass. This keeps the responsibility local
//     ("end_frame allocates what end_frame needs") and avoids confusing
//     dead state during the collect phase.
//   - record_count, instance_count, group_count, submission_counter = 0.
void batch2d_begin_frame(void) {
	arena_reset(&state.arena);

	state.records = arena_alloc(&state.arena, sizeof(batch2d_record_t) * BATCH2D_MAX_SPRITES);
	state.groups  = arena_alloc(&state.arena, sizeof(batch2d_group_t)  * BATCH2D_MAX_SPRITES);

	state.instances          = NULL; // unused field — kept on the struct for now,
	                                 // will remove on the next batch2d cleanup pass
	state.record_count       = 0;
	state.instance_count     = 0;
	state.group_count        = 0;
	state.submission_counter = 0;
}


// Resolves the GL texture id for a 2D material. The diffuse_texture is
// guaranteed non-NULL by material2d_create() (it points to the engine's
// 1x1 white default by construction), so we assert rather than silently
// fall back. A NULL here means the caller actively zeroed the field, which
// is a programming error: zero it on purpose at your own risk.
static uint32_t resolve_texture_id(material2d_t* material) {
	assert(material->diffuse_texture != NULL
		&& "material2d.diffuse_texture is NULL — set a texture or use the default");
	return material->diffuse_texture->id;
}


// Common back-end of all push variants. Writes one record into the arena
// from already-flattened parameters: the caller is responsible for resolving
// model matrix, effective uv_rect, baked color, and texture id.
//
// Centralizing the record write here keeps the sort-key formula and the
// cap-handling in one place — push_object and push_immediate are now thin
// adapters that turn their input shape into these flat parameters.
static void record_sprite(camera2d_t*  camera,
                          int32_t      layer,
                          blend_mode_t blend_mode,
                          uint32_t     texture_id,
                          mat4         model,
                          vec4         uv_rect,
                          color_t      color)
{
	// Catch the most common misuse: submitting a sprite outside of an active
	// frame (e.g. from application_init or application_shutdown). The arena
	// pointers are NULL until batch2d_begin_frame() runs, which happens
	// inside renderer_begin_frame() — meaning sprite_submit / ui_* / text_draw
	// must be called from application_update or later, never from init.
	assert(state.records != NULL
		&& "batch2d: sprite submitted outside of an active frame "
		   "(call from application_update, not application_init)");

	assert(state.record_count < BATCH2D_MAX_SPRITES
		&& "batch2d: per-frame sprite cap reached");

	if (state.records == NULL || state.record_count >= BATCH2D_MAX_SPRITES) {
		return; // release-build safety net
	}

	batch2d_record_t* rec = &state.records[state.record_count];

	rec->camera     = camera;
	rec->blend_mode = blend_mode;
	rec->texture_id = texture_id;

	rec->instance.model   = model;
	rec->instance.uv_rect = uv_rect;
	rec->instance.color   = color;

	rec->sort_key = build_sort_key(
		camera->priority,
		layer,
		blend_mode,
		texture_id,
		state.submission_counter
	);

	state.record_count++;
	state.submission_counter++;
}


// Appends one record for the given retained object. Pulls cached data from
// object2d (the model matrix and the flip-aware uv_rect) so the caller does
// not have to flatten anything.
//
// Behavior at the per-frame cap:
//   - debug build: assert fires (inside record_sprite), the program halts so
//     the offending frame is caught immediately by the developer.
//   - release build (NDEBUG): the sprite is silently dropped. A shipping game
//     must not crash the player over one excess sprite, even in pathological
//     cases.
void batch2d_push_object(object2d_t* obj) {
	material2d_t* material = obj->material;

	uint32_t texture_id = resolve_texture_id(material);

	// Final on-screen tint = per-instance color * material global tint.
	// Multiplied CPU-side (once per sprite per frame) so the fragment shader
	// can stay minimal: just texture_color * vertex_color, no extra uniform.
	color_t obj_color = obj->color;
	color_t mat_color = material->color;
	color_t baked_color = (color_t){
		obj_color.r * mat_color.r,
		obj_color.g * mat_color.g,
		obj_color.b * mat_color.b,
		obj_color.a * mat_color.a,
	};

	record_sprite(
		obj->camera,
		obj->layer,
		material->blend_mode,
		texture_id,
		object2d_get_model_matrix(obj),
		object2d_get_effective_uv_rect(obj),
		baked_color
	);
}


// Appends one immediate sprite. Same machinery as push_object, but the data
// comes straight from the caller instead of being read off an object2d_t.
//
// camera and material are asserted non-NULL — the public sprite_submit()
// in cubit.c guarantees the contract; passing NULL here is a programming
// error (e.g. forgetting to pass camera2d_get_ui()).
void batch2d_push_immediate(camera2d_t* camera,
                            int32_t layer,
                            material2d_t* material,
                            mat4 transform,
                            vec4 uv_rect,
                            color_t color)
{
	assert(camera   != NULL && "batch2d_push_immediate: camera is NULL");
	assert(material != NULL && "batch2d_push_immediate: material is NULL");

	uint32_t texture_id = resolve_texture_id(material);

	// Same CPU-side bake as push_object — keeps shader uniform.
	color_t mat_color = material->color;
	color_t baked_color = (color_t){
		color.r * mat_color.r,
		color.g * mat_color.g,
		color.b * mat_color.b,
		color.a * mat_color.a,
	};

	record_sprite(
		camera,
		layer,
		material->blend_mode,
		texture_id,
		transform,
		uv_rect,
		baked_color
	);
}


// ---- End of frame: sort, group, upload ----

// qsort comparator on the packed sort_key. Pure unsigned compare — no sign
// issues because the bias inside build_sort_key has already pushed signed
// priority/layer values into the non-negative range.
static int compare_records(const void* a, const void* b) {
	uint64_t ka = ((const batch2d_record_t*)a)->sort_key;
	uint64_t kb = ((const batch2d_record_t*)b)->sort_key;
	if (ka < kb) return -1;
	if (ka > kb) return  1;
	return 0;
}


// Single-pass walk over the sorted records that does two things at once:
//   1. extracts instance data (model, uv_rect, color) into three parallel
//      arrays in the arena, in final draw order;
//   2. emits a new batch2d_group_t every time the (camera, blend, texture)
//      tuple changes, so contiguous runs of identical state become one draw.
//
// The three destination arrays are passed in by the caller so this function
// stays focused on building groups; allocations are visible at the call site.
static void build_groups_and_extract(mat4* dst_models,
                                     vec4* dst_uv_rects,
                                     color_t* dst_colors)
{
	if (state.record_count == 0) {
		state.group_count = 0;
		state.instance_count = 0;
		return;
	}

	uint32_t group_count = 0;
	batch2d_group_t* groups = state.groups;

	// Open the first group up-front using the first record's state.
	// All subsequent records either extend this group or trigger a flush
	// that opens a new one.
	const batch2d_record_t* first = &state.records[0];
	groups[group_count] = (batch2d_group_t){
		.camera          = first->camera,
		.blend_mode      = first->blend_mode,
		.texture_id      = first->texture_id,
		.instance_offset = 0,
		.instance_count  = 0,
	};

	for (uint32_t i = 0; i < state.record_count; i++) {
		const batch2d_record_t* rec = &state.records[i];

		// State change → close the current group and open a new one.
		// Camera is compared by pointer because cameras are pooled and
		// stable; texture_id and blend_mode are compared by value.
		bool needs_new_group =
			rec->camera     != groups[group_count].camera     ||
			rec->blend_mode != groups[group_count].blend_mode ||
			rec->texture_id != groups[group_count].texture_id;

		if (needs_new_group) {
			group_count++;
			groups[group_count] = (batch2d_group_t){
				.camera          = rec->camera,
				.blend_mode      = rec->blend_mode,
				.texture_id      = rec->texture_id,
				.instance_offset = i,
				.instance_count  = 0,
			};
		}

		// Extract instance fields into the three SoA upload arrays.
		dst_models  [i] = rec->instance.model;
		dst_uv_rects[i] = rec->instance.uv_rect;
		dst_colors  [i] = rec->instance.color;

		groups[group_count].instance_count++;
	}

	state.group_count    = group_count + 1; // we 0-indexed during the walk
	state.instance_count = state.record_count;
}


// Uploads the three per-attribute arrays to their respective GPU buffers.
// One glBufferSubData per attribute covers the entire frame's instance set,
// regardless of how many groups end up being drawn afterwards.
//
// Uses a single bind per buffer; no VAO bind needed here because
// glBufferSubData targets the buffer object, not the VAO state.
static void upload_instances(const mat4* models,
                             const vec4* uv_rects,
                             const color_t* colors,
                             uint32_t count)
{
	if (count == 0) return;

	glBindBuffer(GL_ARRAY_BUFFER, state.instance_vbo_model);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(mat4) * count, models);

	glBindBuffer(GL_ARRAY_BUFFER, state.instance_vbo_uv_rect);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec4) * count, uv_rects);

	glBindBuffer(GL_ARRAY_BUFFER, state.instance_vbo_color);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(color_t) * count, colors);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


// Closes the production phase: sorts the submitted records, builds the group
// array, and pushes all instance data to the GPU. After this returns, the
// backend can iterate batch2d_get_groups() and emit draws.
//
// Three small temp arrays are allocated from the arena for the SoA upload —
// they live until the next batch2d_begin_frame() resets the arena.
void batch2d_end_frame(void) {
	if (state.record_count == 0) {
		state.group_count = 0;
		state.instance_count = 0;
		return;
	}

	// 1. Sort by packed sort key.
	qsort(state.records, state.record_count, sizeof(batch2d_record_t), compare_records);

	// 2. Allocate per-attribute upload arrays from the arena, then walk the
	//    sorted records once to extract instance data and build groups.
	mat4*    dst_models   = arena_alloc(&state.arena, sizeof(mat4)    * state.record_count);
	vec4*    dst_uv_rects = arena_alloc(&state.arena, sizeof(vec4)    * state.record_count);
	color_t* dst_colors   = arena_alloc(&state.arena, sizeof(color_t) * state.record_count);

	build_groups_and_extract(dst_models, dst_uv_rects, dst_colors);

	// 3. Upload to GPU. One sub-data per attribute, covering all groups.
	upload_instances(dst_models, dst_uv_rects, dst_colors, state.instance_count);
}


// ---- Frame consumption (called by the backend overlay pass) ----

const batch2d_group_t* batch2d_get_groups(void) {
	return state.groups;
}

uint32_t batch2d_get_group_count(void) {
	return state.group_count;
}

void batch2d_bind_quad_vao(void) {
	glBindVertexArray(state.quad_vao);
}

uint32_t batch2d_get_instance_vbo_model(void) {
	return state.instance_vbo_model;
}

uint32_t batch2d_get_instance_vbo_uv_rect(void) {
	return state.instance_vbo_uv_rect;
}

uint32_t batch2d_get_instance_vbo_color(void) {
	return state.instance_vbo_color;
}
