# Cubit Engine — Architecture Notes

## Goal
Custom 3D engine in C with OpenGL. Efficient rendering of 100k+ objects for low-poly and voxel-style games.

## Current Architecture (After Stage 14)

### Rendering Pipeline
- **Two-pass rendering**: shadow pass writes depth from each shadow-casting light's perspective, then main pass renders scene with per-light shadow lookup
- Instanced rendering via `glDrawElementsInstanced` — one draw call per unique mesh+material combination
- Automatic batching: `submit_object3d()` pushes to batch registry, engine groups by mesh+material, uploads transforms and uv_rects per batch
- Frustum culling via AABB: mesh computes local AABB from vertex data at creation, object transforms it to world space, camera tests against 6 frustum planes (Griggs-Hartmann extraction) with configurable margin (`CULLING_BORDER`) to prevent popping at screen edges
- Two-tier uniform push: scene-wide (lights, camera position, ambient factor, sampler slots, shadow maps[0–3], light VPs[0–3]) at shader switch; per-material (colors, shininess, diffuse+normal texture bind) per batch entry
- Arena allocator (`stb_memory.h`, mmap-backed) owns all per-frame batch memory, reset each frame

### Camera System
- **Public API** (`cubit.h`): creation (free/target mode), movement, rotation, zoom, viewport update, active camera set/get
- **Internal API** (`camera.h`): frustum corners extraction, visibility testing, matrix getters, struct definition
- **Active camera pattern**: game calls `camera_set_active()` once per frame before submitting objects. Frontend caches VPM, updates camera position in backend, and extracts frustum corners for shadow mapping. `submit_object3d()` takes no camera parameter — uses active camera implicitly
- Frustum corners accept a far override parameter for shadow distance clamping

### Mesh System
- `mesh_t`: VAO, separate VBOs per attribute (position/normal/UV/tangent), two instance VBOs (transform + uv_rect), EBO, vertex count, index count, local AABB
- Attribute locations: 0=position, 1=normal, 2=UV, 3=tangent, 4=bone_idx(future), 5=bone_wght(future), 6–9=instanced model matrix, 10=instanced uv_rect (vec4)
- `backend_mesh_new` takes nullable arrays — missing attributes simply skip their buffer
- `object3d_t`: transform + mesh pointer + material pointer + world AABB (dirty-flagged, recomputed from mesh AABB × model matrix) + `vec4 uv_rect` (default {0,0,1,1} = full texture, for atlas sub-region)

### Shader System
- `shader_t`: compiled program + `builtin_locations_t` (resolved once at creation) + uniform table (max 16, for custom shaders)
- `builtin_locations_t` caches locations for: vp, surface_color, specular_color, shininess, emissive_color, diffuse_texture sampler, normal_texture sampler, shadow_map[4] samplers, light_vp[4] matrices, light array (16 slots × 11 fields each, including shadow_index), light_count, camera_position, ambient_factor
- Three built-in shaders: **lit** (default, Blinn-Phong + multi-shadow), **unlit** (flat surface_color), **shadow** (depth-only, minimal vertex transform)
- Draw loop reads shader through material chain: `batch_entry → material → shader → locations`

### Material System
- `material_t`: surface_color, specular_color, shininess, roughness (reserved), emissive_color, `texture_t*` diffuse_texture, `texture_t*` normal_texture, `shader_t*` shader
- `material_create()` assigns default shader + default white fallback texture + default flat normal texture automatically
- `object3d_new()` assigns default material at creation — objects are always in a valid, drawable state
- Game sets properties via setters; no OpenGL exposed in API

### Texture System
- `texture_t`: GPU handle (id), width, height, channels, filtering mode, wrapping mode
- `stb_image.h` loads files → `backend_texture_new` uploads to GPU → CPU pixels freed immediately
- 1×1 white fallback texture: always bound when no texture assigned. Shader always samples and multiplies — `surface_color × white = surface_color`, no branches needed
- 1×1 flat normal fallback texture (128, 128, 255): encodes tangent-space (0, 0, 1) — "straight out." TBN × (0,0,1) = vertex normal, no perturbation
- Mipmaps generated for all textures; only used with LINEAR min filter (`GL_LINEAR_MIPMAP_LINEAR`)
- Texture unit allocation: 0=diffuse, 1=normal, 2–5=shadow maps; sampler uniforms set per shader switch
- Channel count (3 or 4) determines `GL_RGB` vs `GL_RGBA` — engine auto-detects from loaded data

### Texture Atlas System
- Atlas is a regular texture loaded via `texture_create()` — no special atlas type needed
- Per-instance `vec4 uv_rect` at attribute location 10 (divisor 1) defines the sub-region within the atlas as normalized UV coordinates (u_min, v_min, u_max, v_max)
- `object3d_set_uv_rect(obj, x, y, w, h)` accepts pixel coordinates (position + dimensions), converts to normalized UVs internally using the material's diffuse texture dimensions
- Vertex shader remaps mesh UVs (0–1) to atlas sub-region: `uv_final = uv * (uv_rect.zw - uv_rect.xy) + uv_rect.xy`
- Default uv_rect is (0, 0, 1, 1) — maps to full texture, zero-cost backward compatibility with non-atlas textures (formula becomes identity)
- Batch registry collects vec4 uv_rects parallel to mat4 transforms, uploaded to a separate per-instance VBO each frame
- Batching key is mesh + material (unchanged): objects sharing the same mesh and atlas-material batch together in one draw call even with different tiles
- Atlas packing is the game developer's responsibility — engine only needs the image and pixel coordinates per sub-region

### Lighting System
- Blinn-Phong forward rendering: ambient + diffuse + specular per light per fragment
- Unified `light_t` struct with type tag: directional, point, spot (all fields present, type controls which matter)
- Light table: 16 slots, compact (shift-on-delete), persistent (not per-frame), `shadow_map_t*` optional pointer per light (NULL default), `shadow_index` (-1 default, assigned per-frame by backend)
- Shader loops active lights, branches on type for direction/attenuation/spot calculations
- Zero-light fallback: flat `base_color` output (backward compatible)
- Ambient factor: scene-level uniform (default 0.1), game-controllable via `ambient_factor_set()`/`ambient_factor_get()`
- Camera position forwarded via simple static overwrite in `backend.c`

### Shadow Mapping System
- **Multi-shadow support**: up to 4 simultaneous shadow-casting lights (`MAX_SHADOW_MAPS 4`), any combination of directional and spot lights
- `shadow_map_t`: FBO handle, depth texture handle (id), size (square, default 8192), light VP matrix, dirty flag
- `shadow.c/h` owns creation, destruction, and VP recalculation; `backend.c` owns OpenGL resource creation (FBO + depth texture)
- **Shadow slot assignment**: `push_scene_data` iterates lights each frame, assigns shadow_index 0–3 to lights with `shadow_map != NULL`, resets others to -1. Assignment happens before light data push so GPU receives correct indices
- **Shadow pass**: `shadow_pass()` in `backend.c` runs before main draw loop, iterates all lights with attached shadow maps, renders scene depth-only using shadow shader with each light's VP
- **Directional light frustum-fitting**: 8 corners of the active camera's frustum (clamped by shadow distance) are transformed into light space, AABB bounds determine orthographic projection. Light positioned at frustum center offset along inverted direction. Z bounds negated/swapped for OpenGL convention
- **Shadow distance**: configurable scene-level value (default 50), controls how far from the camera directional shadows extend. `camera_get_frustum_corners()` receives far override as `min(camera.far, shadow_distance)`. Game-facing getter/setter: `shadow_distance_set()`/`shadow_distance_get()`
- **Spot light projection**: perspective (FOV = 2 × outer_cutoff, aspect 1.0), fixed position and direction — no frustum-fitting needed
- **Border color**: shadow map depth texture uses `GL_CLAMP_TO_BORDER` with border color (1,1,1,1), so fragments outside shadow map coverage read as fully lit
- PCF (Percentage Closer Filtering): 5×5 kernel for soft shadow edges
- Shadow bias (0.005) to prevent shadow acne
- Ambient light unaffected by shadows (only diffuse+specular multiplied by shadow factor)
- Game API: `shadow_map_create()`, `shadow_map_destroy()`, `light_set_shadow_map(index, sm)`, `shadow_distance_set()`/`shadow_distance_get()`

### Init Order
`shader_init()` → `texture_init()` → `light_init()` → `material_init()`. Shutdown reverses.

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

## Key Files
- `backend.c/h` — OpenGL renderer, draw loop, shadow pass, shadow slot assignment, instanced rendering, texture upload/bind, scene+material uniform push, FBO management
- `frontend.c` — Engine API (submit_object3d, camera_set_active, fill_screen, shadow_distance), stb_image/math/memory implementations, active camera and VPM caching
- `shader.c/h` — Shader compilation, builtin location resolution (including shadow_map[4]/light_vp[4]/shadow_index arrays), lit+unlit+shadow shader sources
- `material.c/h` — Material creation with auto shader+texture, property setters
- `texture.c/h` — Texture loading, default fallback, GPU upload via backend
- `light.c/h` — Light table, game-facing light API, shadow map attachment, shadow_index field
- `shadow.c/h` — Shadow map creation/destruction, light VP calculation (frustum-fitted orthographic for directional, perspective for spot)
- `camera.c` — View/projection matrices, frustum planes, visibility testing, frustum corners
- `camera.h` — Internal camera header: struct definition, frustum corners, visibility, matrix getters
- `batch.c/h` — Per-frame batch registry, arena-backed transforms and uv_rects
- `object.c/h` — Object transform, mesh/material pointers, AABB computation, uv_rect with pixel-to-UV conversion
- `mesh.c/h` — Mesh creation, local AABB from vertex data
- `cubit.c` — Main loop
- `cubit_types.h` — Types, forward declarations, enums
- `cubit.h` — Public game API (camera creation/movement/zoom, object submission, materials, lights, shadows, ambient, shadow distance, atlas uv_rect)
- `input.c/h` — Frame-based input system
- `stb_math3d.h` / `stb_memory.h` / `stb_image.h` — Math, arena allocator, image loading
- `game.c` — Test scene: 5 cubes + floor, 3 light types, textured and untextured materials, cobblestone normal map, directional + spot shadow maps, texture atlas with per-object sub-regions

## Future Roadmap
- **Atlas bleeding fix**: padding between atlas tiles and/or UV clamping in shader to prevent texture filtering from sampling adjacent tiles — visible as color fringing at tile edges, especially with LINEAR filtering or mipmap levels
- **Gamma correction / sRGB**: load textures as `GL_SRGB8`/`GL_SRGB8_ALPHA8`, compute lighting in linear space, enable `GL_FRAMEBUFFER_SRGB` for final output — current pipeline operates in raw sRGB which flattens lighting and loses shadow detail
- **Transparency support**: separate opaque/transparent batching, draw opaques first then transparents back-to-front sorted, alpha blending enable/disable per material, alpha cutout for vegetation — batch registry needs opaque/transparent flag and second pass
- **Cascaded Shadow Maps**: split camera frustum into 2–4 distance slices, each with its own shadow map — high resolution near camera, lower resolution far away. Solves shadow distance/quality tradeoff
- **Shadow mapping extensions**: point light shadows (cubemap, 6-face depth pass)
- **Skeletal animation**: bone index/weight buffers (locations 4–5 reserved), bone matrix array
- **2D pipeline**: separate path for sprites, orthographic, layer ordering, sprite atlas batching
