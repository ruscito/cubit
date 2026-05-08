# Cubit Engine — Architecture Notes

## Goal
Custom 3D engine in C with OpenGL. Efficient rendering of 100k+ objects for low-poly and voxel-style games. Includes a complete unified 2D pipeline (sprites, UI, text) sharing the same batching architecture as the 3D pipeline. After Stage 23 the 3D and 2D APIs are symmetric: pool-based retained objects, sort-key-driven batches, per-instance color, single point of contact with the rendering pipeline.

## Current Architecture (After Stage 23)

### Rendering Pipeline (3D)
- **Three-pass 3D rendering**: shadow pass writes depth from each shadow-casting light's perspective, opaque pass renders solid objects with per-light shadow lookup, transparent pass renders translucent objects back-to-front with alpha blending
- **Sort-key-driven batch** (Stage 23): `batch_push_object()` records every active object into a per-frame arena with a packed `uint64_t` sort key `[pass:2][camera_priority:14][material_id:16][mesh_id:16][depth_bucket:16]`. A single `qsort` call separates opaque from transparent, then groups by (material, mesh) for the opaque pass and orders back-to-front (via the depth bucket) for the transparent pass. Same pattern as `batch2d` — produces flat group/instance arrays the backend iterates linearly
- **AoS records, SoA upload**: per-record arena entries collect transform/uv_rect/color, then a SoA split copies them into three GPU buffers per group (transform VBO at locations 6–9, uv_rect at 10, color at 11)
- **Per-instance color** (Stage 23): `obj.color × material.surface_color` baked CPU-side into the instance stream, mirroring the 2D pipeline. The `surface_color` uniform was removed from the lit and unlit shaders; the per-instance `aColor` attribute carries it. Hundreds of cubes sharing one mesh+material can render in a single draw call with different colors
- **Game-side single point of contact** (Stage 23): the game calls `camera_set_active(c)` once per frame and the engine does the rest. `submit_object3d()` is gone — every active object3d in the pool with a mesh and material is collected, frustum-culled, and routed automatically. Mirrors how the 2D pipeline works since Stage 22
- Frustum culling via AABB, two-tier uniform push (scene-wide at shader switch, per-material at group entry)
- Arena allocator (`stb_memory.h`, mmap-backed) owns all per-frame batch memory, reset each frame
- **Transparency**: routed via `material->opacity < 1.0` to the transparent pass class via the sort key's high bits; drawn back-to-front via the depth-bucket low bits, with depth-write off and alpha blending. Alpha cutout via `discard` when `tex_color.a < 0.5`

### Rendering Pipeline (2D — added in Stage 22)
- **Single overlay pass** runs after the 3D transparent pass
- **Unified 2D batch (`batch2d`)**: gameplay sprites, UI primitives, text glyphs, and any future 2D producer flow through one registry. Sort key `(camera priority → layer → blend mode → texture → submission order)` packed into a single `uint64_t`
- **Single shared sprite quad** centered on origin. One VAO, three instanced VBOs (model mat4, uv_rect vec4, color vec4) sized for `BATCH2D_MAX_SPRITES = 20000`
- **Single sprite shader**: vertex `projection * view * aModel * aPos` with UV remapping; fragment `texture * srgb_to_linear(frag_color)` plus `discard` for `tex.a < 0.01`. After Stage 23 the per-instance color is sRGB-linearized in the fragment shader so 2D and 3D match WYSIWYG
- **Three blend modes**: `BLEND_NORMAL`, `BLEND_ADDITIVE`, `BLEND_MULTIPLY`
- **Frame timing**: `batch2d_begin_frame()` and `batch_begin_frame()` (3D) both run in `renderer_begin_frame()` *before* `application_update`. The two collect steps (`object3d_collect_into_batch`, `object2d_collect_into_batch`) and the two close-of-frame calls (`batch_end_frame`, `batch2d_end_frame`) all run in `renderer_draw()` immediately before the relevant pass

### Color Pipeline / sRGB (Stage 23)
- **WYSIWYG convention**: a `color_t` value written by the game is interpreted as sRGB — the value the developer "sees" in their head. `(0.686, 0.843, 0.373)` (CUBIT_GREEN) lands on screen as that exact sRGB pixel, identical between a 3D cube tinted with that color and a `ui_rect_filled(... CUBIT_GREEN ...)` next to it
- **Linearization in three shaders** (sprite, lit, unlit): the `srgb_to_linear()` GLSL helper applies the exact piecewise IEC 61966-2-1 curve (not the `pow(2.2)` approximation) to per-instance color, emissive color, specular color, and per-light color before any math
- **Texture handling unchanged**: color textures upload as `GL_SRGB8` / `GL_SRGB8_ALPHA8` (GPU auto-linearizes on sample), data textures as plain `GL_RGB8` / `GL_RGBA8`. `GL_FRAMEBUFFER_SRGB` enabled so the framebuffer applies the linear→sRGB encode at write time. Lighting math runs in linear space throughout
- The `surface_color` uniform was retired from the built-in lit/unlit shaders; per-instance color subsumes it. Custom shaders that still declare the uniform continue to receive `material->surface_color` from `push_shader_data`

### Object 3D System (Stage 23 — pool-based)
- **`object3d_t`**: position (vec3), rotation (vec3 of euler radians, X then Y then Z), scale (vec3), color (color_t), uv_rect (vec4), mesh + material pointers, active flag, cached transform mat4, cached world-space AABB
- **Pool pattern**: array of `MAX_OBJECTS3D = 10000` slots, stable pointers via `active` flag, identical to `object2d`
- **No dirty flag**: every setter recomputes transform and AABB immediately. Setter cost (matrix multiply + 8-corner AABB transform) is negligible at our object counts; if a future workload calls setters in a hot inner loop, a dirty flag can be reintroduced without changing the public API
- **Defaults**: `object3d_new()` returns an active slot with default material assigned; the game just sets mesh, position, color, and the object renders
- **No `submit_object3d`**: removed from the public API. `object3d_collect_into_batch()` walks the pool every frame, filters by frustum visibility against the active camera, and pushes to the batch
- **`object3d_set_active(false)`**: disables an object without freeing the slot — efficient for object pooling (projectiles, particles, enemies)

### Object 2D System (added in Stage 22)
- **`object2d_t`**: position (vec2 — center of sprite), size (vec2), rotation (radians), color (color_t), uv_rect (vec4), flip_x/flip_y, material2d pointer, camera2d pointer, layer (int32), active flag, cached model_matrix + dirty flag
- **Pool pattern**: array of 10000 slots, stable pointers via `active` flag
- **Center-pivot mesh** consumption: position is the center of the sprite, model matrix is `translate(position) × rotate × scale(size)`
- **Flip via uv_rect swap**: `object2d_get_effective_uv_rect()` returns coordinates with X/Y components swapped when flip flags are set

### Camera System (3D)
- **Public API** (`cubit.h`): creation (free/target mode), movement, rotation, zoom, viewport update, active camera set/get
- **Internal API** (`camera.h`): frustum corners extraction, visibility testing, matrix getters
- **Active camera pattern**: `camera_set_active()` once per frame; the engine reads the active camera implicitly during `object3d_collect_into_batch`, `batch_set_camera`, and CSM cascade computation. Stage 23 wired `camera_set_active` to also push VP and camera position into the 3D batch
- `camera_get_frustum_corners(camera_t*, vec3*, near_dist, far_dist)` — used both for full frustum and per-cascade slices

### Camera System (2D — added in Stage 22)
- **`camera2d_t`**: position, zoom, rotation, viewport_size, priority
- **Pool pattern**: array of 8 cameras, `active` flag, pointer-based API
- **Built-in UI camera**: priority 1000, position = viewport/2, world (0,0) = screen (0,0)
- **Three coordinate spaces** documented and separated: framebuffer / window / virtual

### Mesh System
- `mesh_t`: VAO, separate VBOs per attribute (position/normal/UV/tangent), three instance VBOs (transform + uv_rect + color), EBO, vertex count, index count, local AABB
- Attribute locations: 0=position, 1=normal, 2=UV, 3=tangent, 6–9=instanced model matrix, 10=instanced uv_rect, 11=instanced color
- After Stage 23 the `instance_vbo_color` slot is finally consumed: the 3D opaque/transparent passes upload the baked color stream into it

### Collision System
- **3D collision registry**: flat array of `object3d_t*` (max 1000), opt-in. AABB tests, batch query, slide resolution, slab-method raycast
- **2D collision deferred**: object2d has no AABB or collision support — added when concrete 2D gameplay appears

### Shader System
- `shader_t`: compiled program + `builtin_locations_t` + uniform table
- **Four built-in shaders**: **lit** (Blinn-Phong + multi-shadow + per-instance color + sRGB linearization), **unlit** (per-instance color + sRGB linearization), **shadow** (depth-only with alpha cutout), **sprite** (2D — instanced model+uv_rect+color, sRGB linearization, alpha-cutout discard at 0.01)
- The `srgb_to_linear()` GLSL helper is a single string literal concatenated into the lit/unlit/sprite fragment sources at init — no preprocessor required

### Material Systems
- **`material_t`** (3D): surface_color, specular_color, shininess, emissive_color, diffuse_texture, normal_texture, shader, opacity, cast_shadow. After Stage 23 `surface_color` is multiplied with per-instance object color CPU-side; the lit/unlit fragment shaders do not read it as a uniform anymore (custom shaders that declare it still receive the value)
- **`material2d_t`** (2D): color (vec4 tint), diffuse_texture (defaults to 1×1 white), shader, blend_mode

### Texture System
- `texture_t`: GPU handle, dimensions, channels, filtering, wrapping, `TextureTypes` (COLOR_TEXTURE or DATA_TEXTURE)
- **sRGB pipeline**: color textures as `GL_SRGB8`/`GL_SRGB8_ALPHA8`, data textures as plain `GL_RGB8`/`GL_RGBA8`. `GL_FRAMEBUFFER_SRGB` enabled at init
- **Fallbacks**: 1×1 white, 1×1 flat normal (128, 128, 255)
- Mipmaps generated for all textures; texture units 0=diffuse, 1=normal, 2=shadow atlas

### Texture Atlas System
- Per-instance `vec4 uv_rect` at attribute location 10, anti-bleeding via half-texel inset

### Lighting System
- Blinn-Phong forward rendering, unified `light_t` with type tag (directional/point/spot)
- 16-slot light table, persistent, with `cascade_tiles[MAX_CASCADES]` per light
- Light colors are linearized inside the fragment shaders (Stage 23) — game writes sRGB, math runs linear

### Shadow Mapping (Atlas-Based + CSM)
- Single shared depth atlas + single FBO, per-light tile via `glViewport`
- Cascaded Shadow Maps for directional lights (practical split λ=0.5)
- After Stage 23 the shadow pass consumes the same opaque/transparent group arrays the main passes use, via the same `upload_instance_streams` helper

### Init Order (After Stage 23)
Inside `renderer_init`:
`batch_init()` → `shader_init()` → `texture_init()` → `light_init()` → `material_init()` → `material2d_init()` → `batch2d_init()` → `text_init()` → `object3d_init()`.

Then in `cubit.c` after `renderer_init`:
`shadow_atlas_init()` → `camera2d_init()` → `object2d_init()`.

Shutdown reverses everything.

## Completed Stages

1. **Mesh/Instance Split** — `mesh_t` as shared GPU resource, `object3d_t` as lightweight instance
2. **Instanced Rendering** — `glDrawElementsInstanced`, instance VBO with mat4 per instance
3. **Automatic Batching** — batch registry groups by mesh+material, arena allocator, `submit_object3d()` API (later refactored away in Stage 23)
4. **Frustum Culling** — 6 planes from VP matrix, AABB test
5. **Material/Shader System** — handle-based uniform lookup, default material fallback
6. **Vertex Format & Indexed Rendering** — separate VBOs per attribute, EBO, `LocationTypes` enum
7. **Material/Shader Wiring** — material owns shader pointer, `builtin_locations_t` embedded
8. **Lighting** — Blinn-Phong with directional/point/spot lights, lit/unlit built-in shaders
9. **Texture Support** — `texture_t` type, stb_image loading, fallback white texture, mipmaps
10. **Ambient Control** — scene-level `ambient_factor` uniform
11. **Normal Mapping** — tangent VBO at location 3, TBN matrix, normal map sampling
12. **Shadow Mapping** — multi-shadow system, PCF 5×5, two-pass rendering
13. **Shadow Frustum-Fitting & Camera Refactoring** — directional fitted to camera frustum, `camera.h` internal header, simplified `submit_object3d()` using active camera pattern
14. **Texture Atlas** — per-instance vec4 uv_rect at location 10
15. **Atlas Bleeding Fix** — half-texel UV inset
16. **Gamma Correction / sRGB Pipeline** — `TextureTypes` enum, sRGB textures, `GL_FRAMEBUFFER_SRGB`, lighting in linear space *(extended in Stage 23 to also linearize game-supplied colors at fragment-shader entry)*
17. **Transparency Support** — `float opacity` on material, dual batch registry, back-to-front sort, alpha cutout, `bool cast_shadow`
18. **Alpha Cutout in Shadow Pass** — UV-aware shadow shader
19. **Shadow Atlas + CSM** — single shared depth atlas + FBO, dynamic tile array, Cascaded Shadow Maps
20. **AABB Collision System** — `collision.c/h`, AABB tests, raycast, slide resolution
21. **Text Rendering (Bitmap Font)** — `text.c/h` + embedded font atlas. *(Refactored in Stage 22 into a sprite_submit client)*
22. **2D Sprite, UI & Camera System (unified pipeline)** — full 2D layer in 11 sub-steps. New modules `camera2d`, `material2d`, `object2d`, `batch2d`. New shader **sprite**. New backend pass `overlay_pass`. New API `sprite_submit`, `ui_rect_filled/bordered/progress_bar`, `ui_point_in_rect`, `ui_rect_contains_mouse`. Three coordinate spaces (framebuffer/window/virtual). Text refactored to be a sprite_submit client
23. **3D Pipeline Quality Refactor + sRGB Color Fix** *(NEW)* — brings the 3D API in line with the 2D pipeline established in Stage 22 and fixes WYSIWYG color reproduction. **8 sub-steps**: (1) `object3d_t` becomes pool-based with `MAX_OBJECTS3D = 10000` and an `active` flag; (2) `object3d_set_active()` enables/disables without freeing; (3) per-instance `color_t` field, multiplied with `material->surface_color` CPU-side and uploaded in `instance_vbo_color` (location 11) — same flow as 2D, lit/unlit `surface_color` uniform retired; (4) explicit `position`/`rotation` (vec3 euler)/`scale` (vec3) setters and getters, `dirty` flag removed (every setter recomputes transform + AABB immediately); (5) `submit_object3d()` removed from the public API, replaced by `object3d_collect_into_batch()` driven by the engine in `renderer_draw`; (6) the 3D batch rewritten with a packed uint64 sort key `[pass:2][camera_priority:14][material_id:16][mesh_id:16][depth_bucket:16]`, AoS records during collect, SoA streams during upload, group descriptors for the backend — same architecture as `batch2d`; (7) game-supplied colors (per-instance, emissive, specular, light) linearized at fragment-shader entry using the exact IEC 61966-2-1 sRGB curve in three shaders (sprite, lit, unlit), making `(r,g,b)` written in C land on screen as the same sRGB pixel for both 3D cubes and `ui_rect_filled` — verified WYSIWYG; (8) `game.c` rewritten to demonstrate the new API (10×10 grid sharing one mesh + one material with per-instance colors batches into a single draw call), `Notes.md` updated. Net result: the 2D and 3D pipelines now share architecture, conventions and on-screen color behavior

## Key Files
- `backend.c/h` — OpenGL renderer, draw loop (3D opaque + 3D transparent + 2D overlay passes), shadow pass, instanced 3D rendering driven by the new 3D batch groups, `overlay_pass` for 2D, DPI-aware viewport, `batch_begin_frame` and `batch2d_begin_frame` integrated in `renderer_begin_frame`, `object3d_collect_into_batch` and `object2d_collect_into_batch` integrated in `renderer_draw`
- `frontend.c` — Engine API surface (camera_set_active wiring camera→batch, fill_screen, shadow_distance), stb_image/math/memory implementations
- `shader.c/h` — Shader compilation, builtin location resolution, lit+unlit+shadow+sprite shader sources, sRGB linearization helper concatenated into three of them
- `material.c/h` — 3D material creation with auto shader+texture, property setters
- `material2d.c/h` — 2D material (texture+tint+blend), default material singleton
- `texture.c/h` — Texture loading with type, default fallbacks, sRGB format selection, `texture_create_from_memory`
- `light.c/h` — Light table, game-facing light API, shadow enable/disable
- `shadow.c/h` — Shadow atlas, tile management, VP calculation with texel snapping, CSM split distances
- `camera.c/h` — 3D view/projection matrices, frustum planes, visibility testing, frustum corners
- `camera2d.c/h` — 2D camera pool, UI camera built-in, `camera2d_screen_to_virtual`
- `batch.c/h` — 3D per-frame batch with packed uint64 sort key, AoS records, SoA upload, opaque/transparent group descriptors. Same architecture as `batch2d` after Stage 23
- `batch2d.c/h` — 2D per-frame batch with packed uint64 sort key, single shared sprite quad VAO, AoS records, SoA upload, group descriptors
- `object.c/h` — 3D object pool of 10000, stable pointers, position/rotation/scale/color setters, cached transform + world AABB, `object3d_collect_into_batch` for the per-frame collect step
- `object2d.c/h` — 2D retained sprite pool of 10000, stable pointers, cached model matrix, flip via uv_rect swap, `object2d_collect_into_batch`
- `collision.c/h` — 3D collision registry, AABB test, raycast, slide resolution
- `mesh.c/h` — Mesh creation, local AABB from vertex data, three instance VBOs (transform/uv_rect/color)
- `text.c/h` — Text rendering as a client of `sprite_submit`
- `font_atlas.h` — Embedded font atlas
- `input.c/h` — Frame-based input system
- `cubit.c` — Main loop, public API: `sprite_submit`, `ui_*`, `ui_point_in_rect`, `ui_rect_contains_mouse`
- `cubit_types.h` — Types, forward declarations, enums, `app_config_t`, `sprite_submit_t` config struct
- `cubit.h` — Public game API (camera 3D + 2D, object 3D + 2D, material 3D + 2D, sprite_submit, text_draw, UI primitives, hit-test, lighting, textures, shadows, collision)
- `stb_math3d.h` — Math
- `stb_memory.h` — Arena allocator (mmap-backed, POSIX-only for now)
- `stb_image.h` — Image loading
- `game3d.c` / `game2d.c` — Test scenes

## Next Stages — Road to Game-Ready

### Stage 24 — Audio System
Integrate a lightweight audio library for sound effects and background music.
- **Backend**: miniaudio (single-header, cross-platform)
- **Sound API**: `sound_t* sound_load(...)`, `sound_play`, `sound_play_looped`, `sound_stop`, `sound_destroy`
- **Volume control**: per-sound and global master
- **Positional audio** (stretch): `sound_play_at(sound_t*, vec3 position)` with distance attenuation
- **Music**: separate streaming path

### Stage 25 — Model Loading (OBJ)
Stop hardcoding vertex arrays.
- **OBJ parser**: v/vn/vt/f, triangulate quads, compute tangents
- **MTL support** (basic)
- **API**: `model_t* model_load(const char* obj_path)` → one or more mesh+material pairs
- **Vertex deduplication**
- **Future extension point**: glTF later for skeletal animation

### Stage 26 — Scene & Object Management
Structure for many game objects without raw pointer tracking.
- **Object registry**: `object3d_find_by_tag`, iteration
- **Tags and naming**
- **Parent-child hierarchy**: `object3d_set_parent`, world transform via chain walk (cached, dirty-flagged)
- **Scene clear**
- **Per-object update_func** callback option

### Stage 27 — Skeletal Animation
Uses bone slots already reserved at locations 4 and 5.
- **Bone data on mesh**: index (ivec4) + weight (vec4) up to 4 bones/vertex
- **`skeleton_t`** with bone tree
- **`animation_t`** clips, `animation_play/stop/blend`
- **Bone matrix uniform array** (max 64), vertex shader skinning
- **glTF loader**: extends Stage 25

### Stage 28 — Cross-Platform Memory
Replace POSIX-only `mmap`/`munmap`.
- **Platform layer**: `_WIN32` → `VirtualAlloc`/`VirtualFree`, else mmap
- **Build system**: CMakeLists.txt or Makefile with platform detection

## Stretch Goals (Post Game-Ready)
- **Pixel art mode**: render at native low resolution onto an offscreen FBO, upscale with nearest-neighbor
- **Point light shadows**: cubemap depth pass
- **Particle system**: GPU-instanced quads, configurable lifetime/velocity/color curves, plugged into the unified 2D pipeline
- **Post-processing stack**: bloom, SSAO, tone mapping via fullscreen quad passes
- **Spatial partitioning**: octree or grid for collision broadphase
- **2D collision**: AABB tests on `object2d_t`
- **UI input manager**: hover/active/captured state, click events, focus, z-order
- **Networking**: UDP client/server for multiplayer
