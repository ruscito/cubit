# Cubit Engine — Architecture Notes

## Goal
Custom 3D engine in C with OpenGL. Efficient rendering of 100k+ objects for low-poly and voxel-style games.

## Current Architecture (After Stage 21)

### Rendering Pipeline
- **Four-pass rendering**: shadow pass writes depth from each shadow-casting light's perspective, opaque pass renders solid objects with per-light shadow lookup, transparent pass renders translucent objects back-to-front with alpha blending, text pass renders 2D overlay glyphs with orthographic projection
- Instanced rendering via `glDrawElementsInstanced` — one draw call per unique mesh+material combination
- Automatic batching: `submit_object3d()` pushes to batch registry, engine groups by mesh+material, uploads transforms and uv_rects per batch
- Frustum culling via AABB: mesh computes local AABB from vertex data at creation, object transforms it to world space, camera tests against 6 frustum planes (Griggs-Hartmann extraction) with configurable margin (`CULLING_BORDER`) to prevent popping at screen edges
- Two-tier uniform push: scene-wide (lights, camera position, ambient factor, sampler slots, shadow maps[0–3], light VPs[0–3]) at shader switch; per-material (colors, shininess, diffuse+normal texture bind) per batch entry
- Arena allocator (`stb_memory.h`, mmap-backed) owns all per-frame batch memory, reset each frame
- **Transparency**: objects with `material->opacity < 1.0` routed to separate transparent registry. Transparent pass runs after opaque pass with `GL_BLEND` enabled (`SRC_ALPHA`/`ONE_MINUS_SRC_ALPHA`), depth write off (`glDepthMask(GL_FALSE)`), objects drawn individually (no instancing) sorted back-to-front by distance² from camera. Alpha cutout via `discard` in fragment shader when `tex_color.a * opacity < 0.5` applies to all objects (opaque and transparent)

### Camera System
- **Public API** (`cubit.h`): creation (free/target mode), movement, rotation, zoom, viewport update, active camera set/get
- **Internal API** (`camera.h`): frustum corners extraction, visibility testing, matrix getters, struct definition
- **Active camera pattern**: game calls `camera_set_active()` once per frame before submitting objects. Frontend caches VPM, updates camera position in backend, computes cascade split distances, generates per-cascade frustum corners and passes them to backend, and forwards view matrix for CSM depth calculation. `submit_object3d()` takes no camera parameter — uses active camera implicitly
- `camera_get_frustum_corners(camera_t*, vec3*, near_dist, far_dist)` accepts explicit near/far distances — used both for full frustum (spot lights) and per-cascade slices (directional CSM)

### Mesh System
- `mesh_t`: VAO, separate VBOs per attribute (position/normal/UV/tangent), three instance VBOs (transform + uv_rect + color), EBO, vertex count, index count, local AABB
- Attribute locations: 0=position, 1=normal, 2=UV, 3=tangent, 4=bone_idx(future), 5=bone_wght(future), 6–9=instanced model matrix, 10=instanced uv_rect (vec4), 11=instanced color (vec4)
- `backend_mesh_new` takes nullable arrays — missing attributes simply skip their buffer
- `object3d_t`: transform + mesh pointer + material pointer + world AABB (dirty-flagged, recomputed from mesh AABB × model matrix) + `vec4 uv_rect` (default {0,0,1,1} = full texture, for atlas sub-region)

### Collision System
- **Collision registry**: flat array of `object3d_t*` pointers (max 1000), game opts-in objects via `collision_3d_add_collidable()`. No spatial partitioning — linear scan sufficient for <1000 colliders
- **AABB test**: `collision_3d_test(a, b)` — six comparisons (separating axis on X/Y/Z), uses `object3d_get_aabb()` to ensure dirty-flagged AABBs are refreshed before testing
- **Batch query**: `collision_3d_test_all(obj, &iter)` — iterator-based, returns one colliding object per call, skips self, game loops until NULL
- **Slide resolution**: `collision_resolve_slide(a, b)` — computes penetration on each axis from both directions, picks smallest overlap axis, pushes object out via `object3d_set_position()` to keep position and transform matrix synchronized
- **Raycast**: `collision_3d_raycast(origin, direction, max_distance)` — slab method (per-axis interval intersection), handles parallel rays and origin-inside-box cases, returns `raycast_result_t` with closest hit object, hit point, distance, and bool. Used both for gameplay (shooting, picking) and preventive collision (cast before move to avoid penetration)
- **Add/Remove**: `collision_3d_add_collidable()` with duplicate check, `collision_3d_remove_collidable()` with swap-and-pop for O(1) removal
- **Deferred**: trigger zones (is_trigger flag) and collision callbacks deferred until more gameplay code reveals the right design

### Shader System
- `shader_t`: compiled program + `builtin_locations_t` (resolved once at creation) + uniform table (max 16, for custom shaders)
- `builtin_locations_t` caches locations for: vp, surface_color, specular_color, shininess, emissive_color, opacity, diffuse_texture sampler, normal_texture sampler, shadow_atlas sampler, light_vp[MAX_LIGHTS], shadow_rect[MAX_LIGHTS], cascade_vp[MAX_CASCADES], cascade_rect[MAX_CASCADES], cascade_splits[MAX_CASCADES], cascade_count, view_matrix, light array (16 slots × 10 fields each), light_count, camera_position, ambient_factor
- Three built-in shaders: **lit** (default, Blinn-Phong + multi-shadow), **unlit** (flat surface_color), **shadow** (depth-only with alpha cutout — samples diffuse texture via UV + uv_rect remapping, discards fragments with `tex_color.a < 0.5`). Plus **text** shader (orthographic, samples font atlas with per-glyph color tint via instanced color attribute at location 11)
- Draw loop reads shader through material chain: `batch_entry → material → shader → locations`

### Material System
- `material_t`: surface_color, specular_color, shininess, roughness (reserved), emissive_color, `texture_t*` diffuse_texture, `texture_t*` normal_texture, `shader_t*` shader, `float opacity` (default 1.0), `bool cast_shadow` (default true)
- `material_create()` assigns default shader + default white fallback texture + default flat normal texture automatically
- `object3d_new()` assigns default material at creation — objects are always in a valid, drawable state
- Game sets properties via setters; no OpenGL exposed in API
- **Opacity**: determines whether object is opaque (1.0) or transparent (<1.0). Engine uses this to route objects to opaque or transparent batch registries automatically

### Texture System
- `texture_t`: GPU handle (id), width, height, channels, filtering mode, wrapping mode, `TextureTypes type` (COLOR_TEXTURE or DATA_TEXTURE)
- `texture_create(filename, type)` — game specifies whether texture is color (diffuse, emissive) or data (normal map)
- `texture_create_from_memory(pixels, w, h, channels, type)` — creates texture from raw pixel data without loading from disk, used by the text system for the embedded font atlas
- **sRGB pipeline**: color textures uploaded as `GL_SRGB8`/`GL_SRGB8_ALPHA8` (GPU auto-converts to linear on sample), data textures as `GL_RGB8`/`GL_RGBA8` (no conversion). `GL_FRAMEBUFFER_SRGB` enabled at init for automatic linear→sRGB on output. Lighting computed in linear space
- `stb_image.h` loads files → `backend_texture_new` uploads to GPU → CPU pixels freed immediately
- 1×1 white fallback texture: always bound when no texture assigned. Shader always samples and multiplies — `surface_color × white = surface_color`, no branches needed
- 1×1 flat normal fallback texture (128, 128, 255): encodes tangent-space (0, 0, 1) — "straight out." TBN × (0,0,1) = vertex normal, no perturbation
- Mipmaps generated for all textures; only used with LINEAR min filter (`GL_LINEAR_MIPMAP_LINEAR`)
- Texture unit allocation: 0=diffuse, 1=normal, 2=shadow atlas; sampler uniforms set per shader switch
- Channel count (3 or 4) determines external format (`GL_RGB` vs `GL_RGBA`); internal format determined by channel count × texture type

### Texture Atlas System
- Atlas is a regular texture loaded via `texture_create()` — no special atlas type needed
- Per-instance `vec4 uv_rect` at attribute location 10 (divisor 1) defines the sub-region within the atlas as normalized UV coordinates (u_min, v_min, u_max, v_max)
- `object3d_set_uv_rect(obj, x, y, w, h)` accepts pixel coordinates (position + dimensions), applies half-texel inset (±0.5 px before normalization) to prevent filtering from sampling tile edges, then converts to normalized UVs using the material's diffuse texture dimensions
- Vertex shader remaps mesh UVs (0–1) to atlas sub-region: `uv_final = uv * (uv_rect.zw - uv_rect.xy) + uv_rect.xy`
- Default uv_rect is (0, 0, 1, 1) — maps to full texture, zero-cost backward compatibility with non-atlas textures (formula becomes identity)
- Batch registry collects vec4 uv_rects parallel to mat4 transforms, uploaded to a separate per-instance VBO each frame
- Batching key is mesh + material (unchanged): objects sharing the same mesh and atlas-material batch together in one draw call even with different tiles
- Atlas packing is the game developer's responsibility — engine only needs the image and pixel coordinates per sub-region
- **Anti-bleeding**: two-layer defense. (1) Asset pipeline: atlas tiles must include border padding (2+ px of extended edge pixels) to survive mipmap blending — game dev responsibility (Aseprite export or external tool). (2) Engine: `object3d_set_uv_rect` insets UV coords by half a texel so filtering never samples exactly on the tile boundary

### Lighting System
- Blinn-Phong forward rendering: ambient + diffuse + specular per light per fragment
- Unified `light_t` struct with type tag: directional, point, spot (all fields present, type controls which matter)
- Light table: 16 slots, compact (shift-on-delete), persistent (not per-frame), `int32_t cascade_tiles[MAX_CASCADES]` per light (all default -1), `int32_t cascade_count` (0 = no shadow, 1 = spot/point, MAX_CASCADES = directional CSM)
- Shader loops active lights, branches on type for direction/attenuation/spot calculations
- Zero-light fallback: flat `base_color` output (backward compatible)
- Ambient factor: scene-level uniform (default 0.1), game-controllable via `ambient_factor_set()`/`ambient_factor_get()`
- Camera position forwarded via simple static overwrite in `backend.c`

### Shadow Mapping System (Atlas-Based + CSM)
- **Shadow atlas**: all shadow-casting lights share a single depth texture and single FBO. Each light writes to a tile region via `glViewport`. Configurable via `app_config_t`: `shadow_atlas_size` (default 8192) and `shadow_tile_size` (default 2048). Tile count = `(atlas_size/tile_size)²`, dynamically allocated
- **`shadow_tile_t`**: pixel position (x, y) for `glViewport`, tile size, normalized `vec4 rect` for shader sampling, `mat4 vp` (light view-projection), `bool occupied`
- **`shadow_atlas_t`**: single FBO, single depth texture, atlas/tile sizes, tile count, `malloc`'d tile array, `float lambda` (practical split blend, default 0.5), `float split_distances[MAX_CASCADES + 1]` (recomputed each frame)
- **Shadows as light property**: `light_t` has `int32_t cascade_tiles[MAX_CASCADES]` (default -1) and `int32_t cascade_count`. No separate shadow type. `light_enable_shadow(index)` auto-detects light type: allocates 1 tile for spot/point (`cascade_count = 1`), MAX_CASCADES tiles for directional (`cascade_count = MAX_CASCADES`). Rollback on insufficient tiles. `light_disable_shadow(index)` releases all tiles
- **Cascaded Shadow Maps (CSM)**: directional lights use multiple atlas tiles, one per frustum cascade. Practical split scheme (`lambda * log + (1-lambda) * linear`) divides the camera frustum from near to shadow_distance into `cascade_count` slices. Frontend computes split distances and per-cascade frustum corners each frame in `camera_set_active`, passes them to backend. Shadow pass loops cascades per directional light: each cascade gets its own frustum corners → AABB fitting → orthographic VP → tile render
- **CSM fragment shader**: dedicated `calculate_cascade_shadow()` computes view-space depth via `view_matrix`, selects cascade by comparing against `cascade_splits[]`, projects and samples from the correct cascade tile. Directional lights use this path; spot/point use the original `calculate_shadow(i)` path via `shadow_rect[i]`
- **CSM uniforms**: `cascade_vp[MAX_CASCADES]`, `cascade_rect[MAX_CASCADES]`, `cascade_splits[MAX_CASCADES]`, `cascade_count`, `view_matrix`. Injected alongside `MAX_CASCADES` define. `push_scene_data` finds first directional light with cascades, uploads its data, zeros its `shadow_rect[i]` so the old path skips it. One CSM light supported
- **Texel snapping**: VP matrix translation (`m[12]`, `m[13]`) rounded to nearest texel boundary after AABB fitting, prevents shadow swimming on camera movement
- **Shadow pass**: bind atlas FBO once, clear once. Per light: loop `cascade_count` times, compute VP per cascade (directional uses cascade corners, spot uses full frustum), set `glViewport` to tile region, render scene. Spot/point with `cascade_count = 1` loops once — same behavior as before
- **Spot light projection**: perspective (FOV = 2 × outer_cutoff, aspect 1.0), fixed position and direction — no frustum-fitting needed
- **Border color**: atlas depth texture uses `GL_CLAMP_TO_BORDER` with border color (1,1,1,1), so fragments outside coverage read as fully lit
- PCF (Percentage Closer Filtering): 5×5 kernel for soft shadow edges, samples from atlas coordinates
- Shadow bias (0.005) to prevent shadow acne
- Ambient light unaffected by shadows (only diffuse+specular multiplied by shadow factor)
- Game API: `light_enable_shadow(index)`, `light_disable_shadow(index)`, `shadow_distance_set()`/`shadow_distance_get()` — CSM is automatic for directional lights, transparent to game developer

### Init Order
`shader_init()` → `texture_init()` → `light_init()` → `material_init()` → `text_init()` → `backend_text_init()`. Then `shadow_atlas_init()` (called from `cubit.c` after `renderer_init`). Shutdown reverses.

## Completed Stages

1. **Mesh/Instance Split** — `mesh_t` as shared GPU resource, `object3d_t` as lightweight instance
2. **Instanced Rendering** — `glDrawArraysInstanced` (later upgraded to `glDrawElementsInstanced`), instance VBO with mat4 per instance
3. **Automatic Batching** — batch registry groups by mesh+material, arena allocator for per-frame transforms, `submit_object3d()` API
4. **Frustum Culling** — 6 planes from VP matrix, AABB test, dirty-flagged world AABB on objects
5. **Material/Shader System** — handle-based uniform lookup, default material fallback, batch grouping by mesh+material
6. **Vertex Format & Indexed Rendering** — separate VBOs per attribute, EBO, vertex colors removed, `LocationTypes` enum
7. **Material/Shader Wiring** — material owns shader pointer, `builtin_locations_t` embedded in shader, draw loop follows material→shader→locations chain
8. **Lighting** — Blinn-Phong with directional/point/spot lights, unified light struct, two-tier uniform push, lit/unlit built-in shaders
9. **Texture Support** — `texture_t` type, stb_image loading, fallback white texture, sampler uniforms, texture×surface_color multiply in shader, mipmaps
10. **Ambient Control** — promoted hardcoded 0.1 ambient to scene-level `ambient_factor` uniform, game-facing getter/setter API
11. **Normal Mapping** — tangent VBO at location 3, TBN matrix in vertex shader, normal map sampling in fragment shader, 1×1 flat-normal fallback texture, guard for meshes without tangent data, normal texture on unit 1
12. **Shadow Mapping** — multi-shadow system (up to 4 simultaneous shadow maps), per-frame slot assignment, directional (orthographic) + spot (perspective) shadow projection, PCF 5×5, shadow_index per light in GLSL, two-pass rendering with per-light depth pass
13. **Shadow Frustum-Fitting & Camera Refactoring** — directional light projection fitted to camera frustum (AABB from 8 corners in light space), shadow distance (default 50) controls coverage range, border color fix for out-of-bounds samples, culling margin (`CULLING_BORDER 1.5`) to prevent object popping at screen edges, `camera.h` internal header separating engine internals from public API, simplified `submit_object3d()` using active camera pattern
14. **Texture Atlas** — per-instance vec4 uv_rect at location 10, shader-based UV remapping, game dev provides atlas image + pixel-coordinate sub-regions, engine converts to normalized UVs, objects with different tiles but same mesh+material batch together in one draw call
15. **Atlas Bleeding Fix** — half-texel UV inset in `object3d_set_uv_rect` (±0.5 px before normalization), atlas padding (extended border pixels) as asset pipeline responsibility
16. **Gamma Correction / sRGB Pipeline** — `TextureTypes` enum (COLOR_TEXTURE/DATA_TEXTURE) on `texture_t`, color textures as `GL_SRGB8`/`GL_SRGB8_ALPHA8`, data textures as `GL_RGB8`/`GL_RGBA8`, `GL_FRAMEBUFFER_SRGB` enabled at init, lighting now computed in linear space
17. **Transparency Support** — `float opacity` on material (default 1.0), dual batch registry (opaque instanced + transparent individual), transparent pass with alpha blending (`SRC_ALPHA`/`ONE_MINUS_SRC_ALPHA`), depth read yes/write no, back-to-front sorting by distance² from camera, alpha cutout via `discard` when `tex_color.a * opacity < 0.5`, fragment output alpha = `tex_color.a * opacity`, `bool cast_shadow` on material (default true) allows game dev to disable shadow casting for transparent objects
18. **Alpha Cutout in Shadow Pass** — shadow shader upgraded from empty depth-only to UV-aware: accepts UV (location 2) + uv_rect (location 10), remaps UVs, samples diffuse texture, discards fragments with `tex_color.a < 0.5`. Shadow pass in backend binds each entry's diffuse texture before draw call (both opaque and transparent loops). Shadows of cutout objects (leaves, fences) now have correct holes instead of solid silhouettes
19. **Shadow Atlas + CSM** — refactored from separate per-light FBO+texture to single shared depth atlas. `shadow_map_t` eliminated — replaced by `int32_t cascade_tiles[MAX_CASCADES]` + `cascade_count` on `light_t`. Dynamic tile array (`malloc`'d from config). Atlas FBO bound once per shadow pass, `glViewport` per tile. Fragment shader samples single `shadow_atlas` sampler, remaps coordinates via per-light `shadow_rect[MAX_LIGHTS]`. Shader defines (`MAX_LIGHTS`, `MAX_CASCADES`, light type enums) injected from C. Game API simplified: `light_enable_shadow`/`light_disable_shadow` auto-detect light type — spot gets 1 tile, directional gets MAX_CASCADES tiles. Cascaded Shadow Maps: practical split scheme (λ=0.5) divides frustum into slices, each rendered into its own atlas tile. Frontend computes per-cascade frustum corners each frame. Dedicated GLSL path (`calculate_cascade_shadow`) selects cascade by view-space depth via `view_matrix` uniform. Texel snapping on VP translation prevents shadow swimming. `MAX_CASCADES` defined in `cubit_types.h`. CSM transparent to game developer
20. **AABB Collision System** — `collision.c/h` module. Flat registry of collidable `object3d_t*` pointers (max 1000), game opts-in via `collision_3d_add_collidable()` with duplicate check, `collision_3d_remove_collidable()` with swap-and-pop. `collision_3d_test(a, b)` performs 6-comparison separating axis test using `object3d_get_aabb()` for dirty-safe AABB access. `collision_3d_test_all(obj, &iter)` iterator-based batch query skipping self. `collision_resolve_slide(a, b)` computes per-axis penetration from both directions, pushes object out along smallest overlap axis via `object3d_set_position()` keeping position/transform in sync. `collision_3d_raycast(origin, dir, max_dist)` implements slab method — per-axis interval intersection, handles parallel rays and origin-inside-box, returns `raycast_result_t` (object, point, distance, hit bool) for closest hit. Raycast also used preventively before movement to avoid visible penetration frames. `object3d_move()` added for relative position updates. Trigger zones and collision callbacks deferred pending more gameplay experience
21. **Text Rendering (Bitmap Font)** — `text.c/h` module + `font_atlas.h` embedded PNG. DejaVu Sans Mono 24px rasterized into 512×192 atlas (16×6 grid, 32×32 cells, baseline-aligned). Atlas PNG embedded as `static const uint8_t[]` in header, decoded at init via `stbi_load_from_memory()`, uploaded as `DATA_TEXTURE` (no sRGB conversion). `text_draw(str, length, size, x, y, color)` queues glyphs into static arrays (transforms + uv_rects + colors, max 1024 per frame). Per-glyph `get_uv_rect()` computes normalized UV coordinates with half-texel inset. Dedicated text shader: vertex takes position (loc 0), UV (loc 2), instanced model matrix (loc 6–9), instanced uv_rect (loc 10), instanced color (loc 11); fragment samples atlas, multiplies by per-glyph color, discards low-alpha. `text_pass()` in backend: orthographic projection (origin top-left, Y down), depth test off, cull face off, alpha blending on, single instanced draw call for all queued glyphs, resets glyph count at end of frame. `backend_text_init()` sets up per-instance color VBO (location 11, divisor 1) on the text quad mesh. `texture_create_from_memory()` added to create textures from raw pixel data without file I/O

## Key Files
- `backend.c/h` — OpenGL renderer, draw loop (opaque + transparent + text passes), shadow pass (atlas-based, single FBO bind + per-cascade viewport loop), instanced rendering, texture upload/bind, scene+material uniform push (shadow atlas + per-light VP/rect + cascade VP/rect/splits), alpha blending state management, `backend_text_init`/`backend_text_shutdown` for text color VBO setup
- `frontend.c` — Engine API (submit_object3d, camera_set_active with cascade corner computation, fill_screen, shadow_distance), stb_image/math/memory implementations, active camera and VPM caching
- `shader.c/h` — Shader compilation, builtin location resolution (shadow_atlas sampler, light_vp[MAX_LIGHTS], shadow_rect[MAX_LIGHTS], cascade_vp/rect/splits[MAX_CASCADES], cascade_count, view_matrix), lit+unlit+shadow+text shader sources, dynamic define injection for lit shader
- `material.c/h` — Material creation with auto shader+texture, property setters
- `texture.c/h` — Texture loading with type (color/data), `texture_create_from_memory` for embedded data, default fallbacks, GPU upload via backend, sRGB format selection
- `light.c/h` — Light table, game-facing light API, `light_enable_shadow`/`light_disable_shadow` (auto-detect type, allocate 1 or MAX_CASCADES tiles), `cascade_tiles[MAX_CASCADES]` + `cascade_count` per light
- `shadow.c/h` — Shadow atlas init/shutdown (struct + tiles + GPU resources via backend), tile management (find free, set occupied), shadow_map_update (VP calculation on tile, texel snapping), shadow_compute_split_distances (practical split scheme), shadow_atlas_get
- `camera.c` — View/projection matrices, frustum planes, visibility testing, frustum corners
- `camera.h` — Internal camera header: struct definition, frustum corners, visibility, matrix getters
- `batch.c/h` — Per-frame batch registry (opaque: instanced entries grouped by mesh+material; transparent: individual entries sorted back-to-front by distance²), arena-backed transforms and uv_rects
- `object.c/h` — Object transform, mesh/material pointers, AABB computation, uv_rect with pixel-to-UV conversion, `object3d_move()` for relative position updates
- `collision.c/h` — Collision registry (flat array of collidable object pointers), AABB test, iterator-based batch query, slide resolution, slab-method raycast with `raycast_result_t`, add/remove with duplicate check and swap-and-pop
- `mesh.c/h` — Mesh creation, local AABB from vertex data
- `text.c/h` — Text rendering module: embedded font atlas decode, glyph batching (transforms + uv_rects + colors), `text_draw()` API, getters for backend consumption, per-frame reset
- `font_atlas.h` — Embedded DejaVu Sans Mono font atlas as `static const uint8_t[]` PNG data (512×192, 32×32 cells, baseline-aligned, 95 ASCII glyphs)
- `cubit.c` — Main loop, shadow_atlas_init/shutdown calls
- `cubit_types.h` — Types, forward declarations, enums (including TextureTypes), MAX_CASCADES, app_config_t with shadow atlas config
- `cubit.h` — Public game API (camera creation/movement/zoom, object submission, materials with opacity, lights with enable/disable shadow, ambient, shadow distance, atlas uv_rect, texture creation with type, text_draw)
- `input.c/h` — Frame-based input system
- `stb_math3d.h` / `stb_memory.h` / `stb_image.h` — Math, arena allocator, image loading
- `game.c` — Test scene: 5 cubes + floor, 3 light types, textured and untextured materials, cobblestone normal map, directional + spot shadows via light_enable_shadow, texture atlas with per-object sub-regions, collision testing with movable center cube (IJKL), slide resolution and preventive raycast, text rendering test

## Next Stages — Road to Game-Ready

### Stage 22 — 2D Sprite & UI Layer
Extend the orthographic overlay from Stage 21 into a general-purpose 2D system for HUD elements, menus, and sprite-based gameplay.
- **Sprite API**: `sprite_t` with position, size, rotation, color tint, uv_rect (atlas sub-region), layer (z-order). `sprite_draw(sprite_t*)` submits to the 2D pass
- **Layer sorting**: sprites sorted by layer integer, then by submission order within same layer. Transparent by default (alpha blending always on in 2D pass)
- **UI primitives**: filled rectangles, bordered rectangles, progress bars — all built from textured quads with a 1×1 white texture (same trick as 3D fallback). No full UI framework, just enough building blocks for health bars, score displays, simple menus
- **Screen-space input mapping**: `ui_rect_contains(x, y, w, h, mouse_x, mouse_y)` helper so the game can detect mouse clicks on UI elements using the existing input system
- **Resolution independence**: UI coordinates expressed in virtual units, scaled to actual window size. `ui_set_virtual_resolution(width, height)` called once at init

### Stage 23 — Audio System
Integrate a lightweight audio library for sound effects and background music. Minimal API surface — the goal is "play sounds" not "build a DAW."
- **Backend**: miniaudio (single-header, cross-platform, no dependencies — matches the stb philosophy already used for image/math/memory)
- **Sound API**: `sound_t* sound_load(const char* filename)` (WAV and OGG), `sound_play(sound_t*)`, `sound_play_looped(sound_t*)`, `sound_stop(sound_t*)`, `sound_destroy(sound_t*)`
- **Volume control**: per-sound `sound_set_volume(sound_t*, float)` and global `audio_master_volume(float)`
- **Positional audio** (optional stretch): `sound_play_at(sound_t*, vec3 position)` with distance attenuation relative to active camera position — nice for 3D games, skippable for 2D
- **Music**: separate `music_play(const char* filename)` path for streaming long tracks without loading entirely into memory

### Stage 24 — Model Loading (OBJ)
Stop hardcoding vertex arrays in game.c. Load standard 3D model files at runtime so artists can produce content with Blender, MagicaVoxel, or any modeling tool.
- **OBJ parser**: handle v/vn/vt/f lines, triangulate quads, compute tangents for normal mapping compatibility. OBJ chosen for simplicity — it's a text format, no external dependencies needed
- **MTL support** (basic): parse material name, diffuse color, diffuse texture path — auto-create `material_t` and `texture_t` per material group
- **API**: `model_t* model_load(const char* obj_path)` → returns a struct containing one or more mesh+material pairs. `model_get_mesh(model_t*, uint32_t index)`, `model_get_material(model_t*, uint32_t index)`, `model_get_count(model_t*)`
- **Vertex deduplication**: OBJ indexes position/normal/UV independently, OpenGL needs unified vertices — build a hash map during loading to avoid duplicate vertices
- **Future extension point**: glTF support later for skeletal animation data (bones, weights, keyframes). OBJ gets us loading geometry now without a massive parser

### Stage 25 — Scene & Object Management
Provide structure for managing many game objects without the game developer tracking raw pointers in global arrays.
- **Object registry**: engine-owned flat array of `object3d_t*`, automatic registration on `object3d_new()`, removal on `object3d_destroy()`. Provides `object3d_find_by_tag(const char* tag)` and iteration
- **Tags and naming**: `object3d_set_tag(obj, "enemy")`, `object3d_set_name(obj, "goblin_03")` — simple strings for identification, no complex ECS
- **Parent-child hierarchy**: `object3d_set_parent(child, parent)` — child transform is relative to parent. Engine computes world transform by walking up the chain (cached, dirty-flagged). Enables grouped movement (a sword follows the player, wheels follow the car)
- **Object enable/disable**: `object3d_set_active(obj, bool)` — inactive objects skip rendering, collision, and update. Cheaper than destroy/recreate for object pooling
- **Scene clear**: `scene_clear()` destroys all registered objects — useful for level transitions
- **Fixed update integration**: `object3d_set_update(obj, update_func)` optional per-object callback invoked during `application_fixed_update`. Not mandatory — game can still manage updates manually

### Stage 26 — Skeletal Animation
Bring characters to life. Uses the bone index/weight attribute slots already reserved at locations 4 and 5.
- **Bone data on mesh**: `mesh_create()` extended to accept bone index (ivec4) and bone weight (vec4) arrays — up to 4 bones per vertex. Backward compatible: NULL bone arrays simply skip the VBOs (existing behavior)
- **Skeleton**: `skeleton_t` owns a tree of `bone_t` (name, parent index, inverse bind matrix). Created from model data (glTF or custom binary format)
- **Animation clips**: `animation_t` contains keyframes (position, rotation, scale per bone per timestamp). `animation_play(skeleton, clip, loop)`, `animation_stop(skeleton)`, `animation_blend(skeleton, clip_a, clip_b, factor)` for transitions
- **Bone matrix upload**: final bone matrices (current_pose × inverse_bind) pushed as a uniform array (max 64 bones). Vertex shader: `skinned_pos = bone_matrices[idx.x] * pos * weight.x + bone_matrices[idx.y] * pos * weight.y + ...`
- **glTF loader**: extend Stage 24's model loader to parse glTF binary (`.glb`) — geometry + skeleton + animation clips in one file. OBJ remains supported for static meshes

### Stage 27 — Cross-Platform Memory
Replace POSIX-only `mmap`/`munmap` in `stb_memory.h` with a platform abstraction so Cubit compiles on Windows.
- **Platform layer**: `#ifdef _WIN32` → `VirtualAlloc`/`VirtualFree`, else → `mmap`/`munmap`. Same API surface (`arena_create`, `arena_alloc`, `arena_reset`, `arena_destroy`), different backing implementation
- **Build system**: add a minimal CMakeLists.txt or Makefile with platform detection. Currently implicit "just compile all .c files" — needs formalization for cross-compilation
- **GLFW already cross-platform**: the windowing/input layer (assumed GLFW from the callback signatures) works on Windows/macOS/Linux unchanged
- **Future**: macOS Metal backend, Vulkan backend — but OpenGL 3.3+ works everywhere for now

## Stretch Goals (Post Game-Ready)
- **Point light shadows**: cubemap depth pass (6 faces per light), shadow_index extended to cubemap samplers
- **Particle system**: emitter-based, GPU-instanced quads, configurable lifetime/velocity/color curves — smoke, fire, sparks, dust
- **Post-processing stack**: bloom, SSAO, tone mapping via fullscreen quad passes with FBO ping-pong
- **Spatial partitioning**: octree or grid for collision broadphase — needed when collidable object count exceeds ~1000
- **Networking**: UDP client/server for multiplayer — way down the line, but worth noting as a direction
