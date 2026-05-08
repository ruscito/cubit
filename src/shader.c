// shader.c

#include "shader.h"
#include "stb_math3d.h"

#include <glad/glad.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// ---- sRGB linearization helper, replicated in every fragment shader ------
//
// Engine convention (Stage 23):
//   - Game-supplied RGB values (object3d color, material surface_color,
//     emissive_color, light color, sprite/material2d color) are interpreted
//     as sRGB — the values the developer "sees" when they write 0.2 or 0.8
//     in code.
//   - Color textures are uploaded as GL_SRGB8 / GL_SRGB8_ALPHA8 — OpenGL
//     auto-linearizes them at sample time. Data textures (normals, masks,
//     fonts) stay in plain RGB and are NOT linearized.
//   - GL_FRAMEBUFFER_SRGB is enabled, so the framebuffer applies the final
//     linear → sRGB conversion at write time.
//   - Inside the shader, lighting math runs in linear space.
//
// Net effect: a `(0.2, 0.8, 0.2)` written in C lands on the screen as the
// sRGB pixel (0.2, 0.8, 0.2) — WYSIWYG, identical between 3D cubes and
// 2D ui_rect_filled.
//
// We use the exact piecewise sRGB curve (IEC 61966-2-1), not the pow(2.2)
// approximation. The cost difference is negligible at fragment-shader granularity
// and the accuracy gain matters in the dark end of the gradient.
//
// No shader #include preprocessor (yet) — the helper is duplicated as a
// string literal and concatenated at init.
static const char SRGB_HELPER[] =
	"vec3 srgb_to_linear(vec3 c) {\n"
	"    bvec3 lo = lessThanEqual(c, vec3(0.04045));\n"
	"    vec3 a = pow((c + 0.055) / 1.055, vec3(2.4));\n"
	"    vec3 b = c / 12.92;\n"
	"    return mix(a, b, vec3(lo));\n"
	"}\n";


// ---- Lit (default) shader ------------------------------------------------
//
// Stage 23 changes:
//   - New per-instance attribute aColor (loc 11) carrying the CPU-baked
//     object.color × material.surface_color tint. The fragment shader uses
//     it instead of the surface_color uniform.
//   - srgb_to_linear() applied to the per-instance color, emissive_color,
//     and every light's color before any lighting math.
//   - The surface_color uniform is removed from the lit fragment shader: the
//     per-instance color subsumes it.

static const char default_vs_src[] = "#version 330 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 1) in vec3 aNormal;\n"
	"layout (location = 2) in vec2 aUV;\n"
	"layout (location = 3) in vec3 aTangent;\n"
	"layout (location = 6) in mat4 aModel;\n"
	"layout (location =10) in vec4 aUV_rect;\n"
	"layout (location =11) in vec4 aColor;\n"
	"uniform mat4 vp;\n"
	"out vec3 frag_position;\n"
	"out vec3 frag_normal;\n"
	"out vec3 frag_tangent;\n"
	"out vec3 frag_bitangent;\n"
	"out vec2 frag_uv;\n"
	"out vec4 frag_color;\n"
	"void main() {\n"
	"   vec4 world_pos = aModel * vec4(aPos, 1.0);\n"
	"   frag_position  = world_pos.xyz;\n"
	"   frag_normal    = normalize(mat3(aModel) * aNormal);\n"
	"   frag_tangent   = mat3(aModel) * aTangent;\n"
	"   frag_bitangent = cross(frag_normal, frag_tangent);\n"
	"   frag_uv        = aUV * (aUV_rect.zw - aUV_rect.xy) + aUV_rect.xy;\n"
	"   frag_color     = aColor;\n"
	"   gl_Position    = vp * world_pos;\n"
	"}\0";


static const char default_fs_src[] = "\n"
	"struct Light {\n"
	"   int light_type;\n"
	"   vec3 light_color;\n"
	"   float light_intensity;\n"
	"   vec3 light_direction;\n"
	"   vec3 light_position;\n"
	"   float constant_attenuation;\n"
	"   float linear_attenuation;\n"
	"   float quadratic_attenuation;\n"
	"   float cone_inner_cutoff;\n"
	"   float cone_outer_cutoff;\n"
	"};\n"
	"\n"
	"uniform Light light[MAX_LIGHTS];\n"
	"uniform int   light_count;\n"
	"uniform float ambient_factor;\n"
	"uniform vec3  camera_position;\n"
	"\n"
	// surface_color uniform removed — the per-instance frag_color carries it.
	"uniform vec3  specular_color;\n"
	"uniform float shininess;\n"
	"uniform vec3  emissive_color;\n"
	"uniform float opacity;\n"
	"\n"
	"uniform sampler2D diffuse_texture;\n"
	"uniform sampler2D normal_texture;\n"
	"uniform sampler2D shadow_atlas;\n"
	"uniform mat4  light_vp[MAX_LIGHTS];\n"
	"uniform vec4  shadow_rect[MAX_LIGHTS];\n"
	"uniform mat4  cascade_vp[MAX_CASCADES];\n"
	"uniform vec4  cascade_rect[MAX_CASCADES];\n"
	"uniform float cascade_splits[MAX_CASCADES];\n"
	"uniform int   cascade_count;\n"
	"uniform mat4  view_matrix;\n"
	"\n"
	"in vec3 frag_position;\n"
	"in vec3 frag_normal;\n"
	"in vec3 frag_tangent;\n"
	"in vec3 frag_bitangent;\n"
	"in vec2 frag_uv;\n"
	"in vec4 frag_color;\n"
	"out vec4 fragColor;\n"
	"\n"
	// PCF 5x5 — unchanged from Stage 22
	"float pcf_shadow(vec3 proj) {\n"
	"   float bias = 0.005;\n"
	"   float shadow = 0.0;\n"
	"   vec2 texel_size = vec2(1.0) / vec2(textureSize(shadow_atlas, 0));\n"
	"   for (int x = -2; x <= 2; x++) {\n"
	"       for (int y = -2; y <= 2; y++) {\n"
	"           float depth = texture(shadow_atlas, proj.xy + vec2(x, y) * texel_size).r;\n"
	"           shadow += proj.z - bias > depth ? 1.0 : 0.0;\n"
	"       }\n"
	"   }\n"
	"   return shadow / 25.0;\n"
	"}\n"
	"\n"
	"float calculate_shadow(int i) {\n"
	"   vec4 light_space_pos = light_vp[i] * vec4(frag_position, 1.0);\n"
	"   vec3 proj = light_space_pos.xyz / light_space_pos.w;\n"
	"   proj = proj * 0.5 + 0.5;\n"
	"   if (proj.z > 1.0) return 0.0;\n"
	"   vec4 rect = shadow_rect[i];\n"
	"   proj.x = rect.x + proj.x * (rect.z - rect.x);\n"
	"   proj.y = rect.y + proj.y * (rect.w - rect.y);\n"
	"   return pcf_shadow(proj);\n"
	"}\n"
	"float calculate_cascade_shadow() {\n"
	"   float depth = -(view_matrix * vec4(frag_position, 1.0)).z;\n"
	"   int cascade = cascade_count - 1;\n"
	"   for (int c = 0; c < cascade_count; c++) {\n"
	"       if (depth < cascade_splits[c]) { cascade = c; break; }\n"
	"   }\n"
	"   vec4 light_space_pos = cascade_vp[cascade] * vec4(frag_position, 1.0);\n"
	"   vec3 proj = light_space_pos.xyz / light_space_pos.w;\n"
	"   proj = proj * 0.5 + 0.5;\n"
	"   if (proj.z > 1.0) return 0.0;\n"
	"   vec4 rect = cascade_rect[cascade];\n"
	"   proj.x = rect.x + proj.x * (rect.z - rect.x);\n"
	"   proj.y = rect.y + proj.y * (rect.w - rect.y);\n"
	"   return pcf_shadow(proj);\n"
	"}\n"
	"\n"
	"void main() {\n"
	"   vec3 normal;\n"
	"   if (length(frag_tangent) > 0.001) {\n"
	"       mat3 TBN = mat3(frag_tangent, frag_bitangent, frag_normal);\n"
	"       vec3 map_normal = texture(normal_texture, frag_uv).rgb * 2.0 - 1.0;\n"
	"       normal = normalize(TBN * map_normal);\n"
	"   } else {\n"
	"       normal = normalize(frag_normal);\n"
	"   }\n"
	"   vec3 view_dir = normalize(camera_position - frag_position);\n"
	"\n"
	"   vec4 tex_color = texture(diffuse_texture, frag_uv);\n"
	"   if (tex_color.a < 0.5) discard;\n"
	"\n"
	// Linearize game-supplied colors. Diffuse texture is already linear
	// (sampled from GL_SRGB8 / GL_SRGB8_ALPHA8). Specular and emissive too.
	"   vec3 surface_lin   = srgb_to_linear(frag_color.rgb);\n"
	"   vec3 specular_lin  = srgb_to_linear(specular_color);\n"
	"   vec3 emissive_lin  = srgb_to_linear(emissive_color);\n"
	"\n"
	"   vec3 base_color = surface_lin * tex_color.rgb;\n"
	"\n"
	"   vec3 result = vec3(0.0);\n"
	"   for (int i = 0; i < light_count; i++) {\n"
	"       if (light[i].light_type == LIGHT_OFF) continue;\n"
	"\n"
	"       vec3 light_dir;\n"
	"       float attenuation = 1.0;\n"
	"       float spot_factor = 1.0;\n"
	"\n"
	"       if (light[i].light_type == LIGHT_DIRECTIONAL) {\n"
	"           light_dir = normalize(-light[i].light_direction);\n"
	"       } else {\n"
	"           vec3 to_light = light[i].light_position - frag_position;\n"
	"           float distance = length(to_light);\n"
	"           light_dir = to_light / distance;\n"
	"           attenuation = 1.0 / (light[i].constant_attenuation\n"
	"               + light[i].linear_attenuation * distance\n"
	"               + light[i].quadratic_attenuation * distance * distance);\n"
	"           if (light[i].light_type == LIGHT_SPOT) {\n"
	"               float theta = dot(light_dir, normalize(-light[i].light_direction));\n"
	"               float inner = cos(light[i].cone_inner_cutoff);\n"
	"               float outer = cos(light[i].cone_outer_cutoff);\n"
	"               spot_factor = clamp((theta - outer) / (inner - outer), 0.0, 1.0);\n"
	"           }\n"
	"       }\n"
	"\n"
	"       float diff = max(dot(normal, light_dir), 0.0);\n"
	"       vec3 half_dir = normalize(light_dir + view_dir);\n"
	"       float spec = pow(max(dot(normal, half_dir), 0.0), shininess);\n"
	"       if (diff <= 0.0) spec = 0.0;\n"
	"\n"
	// Light color is also game-supplied → linearize.
	"       vec3 light_lin     = srgb_to_linear(light[i].light_color);\n"
	"       vec3 light_contrib = light_lin * light[i].light_intensity;\n"
	"       vec3 ambient  = ambient_factor * light_contrib * base_color;\n"
	"       vec3 diffuse  = diff * light_contrib * base_color;\n"
	"       vec3 specular = spec * light_contrib * specular_lin;\n"
	"\n"
	"       float shadow_factor = 1.0;\n"
	"       if (light[i].light_type == LIGHT_DIRECTIONAL && cascade_count > 0) {\n"
	"           shadow_factor = 1.0 - calculate_cascade_shadow();\n"
	"       } else if (shadow_rect[i].z > 0.0) {\n"
	"           shadow_factor = 1.0 - calculate_shadow(i);\n"
	"       }\n"
	"\n"
	"       result += ambient + (diffuse + specular) * attenuation * spot_factor * shadow_factor;\n"
	"   }\n"
	"\n"
	"   result += emissive_lin;\n"
	"\n"
	"   if (light_count == 0) {\n"
	"       result = base_color;\n"
	"   }\n"
	"\n"
	"   fragColor = vec4(result, tex_color.a * opacity * frag_color.a);\n"
	"}\0";


// ---- Unlit shader --------------------------------------------------------
//
// Stage 23 changes:
//   - aColor (loc 11) added; surface_color uniform removed.
//   - Linearization applied to the per-instance color so unlit values match
//     the lit pipeline's WYSIWYG behavior.

static const char* unlit_vs_src = "#version 330 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 6) in mat4 aModel;\n"
	"layout (location =11) in vec4 aColor;\n"
	"uniform mat4 vp;\n"
	"out vec4 frag_color;\n"
	"void main() {\n"
	"   gl_Position = vp * aModel * vec4(aPos, 1.0);\n"
	"   frag_color  = aColor;\n"
	"}\0";


static const char unlit_fs_template[] =
	"#version 330 core\n"
	"in vec4 frag_color;\n"
	"out vec4 fragColor;\n"
	"%s" // SRGB_HELPER inserted here
	"void main() {\n"
	"   vec3 lin = srgb_to_linear(frag_color.rgb);\n"
	"   fragColor = vec4(lin, frag_color.a);\n"
	"}";


// ---- Shadow shader -------------------------------------------------------
//
// No color work in the shadow shader — depth only, with alpha cutout. No
// changes from Stage 22.

static const char* shadow_vs_src = "#version 330 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 2) in vec2 aUV;\n"
	"layout (location = 6) in mat4 aModel;\n"
	"layout (location =10) in vec4 aUV_rect;\n"
	"\n"
	"uniform mat4 vp;\n"
	"\n"
	"out vec2 frag_uv;\n"
	"\n"
	"void main() {\n"
	"   gl_Position = vp * aModel * vec4(aPos, 1.0);\n"
	"   frag_uv = aUV * (aUV_rect.zw - aUV_rect.xy) + aUV_rect.xy;\n"
	"}\0";


static const char* shadow_fs_src = "#version 330\n"
	"uniform sampler2D diffuse_texture;\n"
	"in vec2 frag_uv;\n"
	"\n"
	"void main() {\n"
	"   vec4 tex_color = texture(diffuse_texture, frag_uv).rgba;\n"
	"   if (tex_color.a < 0.5) discard;\n"
	"}\0";


// ---- Sprite (2D) shader --------------------------------------------------
//
// Stage 23 changes:
//   - Linearize frag_color before multiplying with texture. Texture is
//     already linear (GL_SRGB8 / GL_SRGB8_ALPHA8 for color textures, plain
//     RGB for fonts/data). The framebuffer reapplies the sRGB encode at
//     write time. Net effect: a 2D ui_rect_filled at (0.2, 0.8, 0.2) lands
//     on screen as the same green a 3D cube with the same color does.

static const char* sprite_vs_src = "#version 330 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 2) in vec2 aUV;\n"
	"layout (location = 6) in mat4 aModel;\n"
	"layout (location = 10) in vec4 aUV_rect;\n"
	"layout (location = 11) in vec4 aColor;\n"
	"\n"
	"uniform mat4 view;\n"
	"uniform mat4 projection;\n"
	"\n"
	"out vec2 frag_uv;\n"
	"out vec4 frag_color;\n"
	"\n"
	"void main() {\n"
	"    gl_Position = projection * view * aModel * vec4(aPos, 1.0);\n"
	"    frag_uv = aUV * (aUV_rect.zw - aUV_rect.xy) + aUV_rect.xy;\n"
	"    frag_color = aColor;\n"
	"}\0";


static const char sprite_fs_template[] =
	"#version 330 core\n"
	"in vec2 frag_uv;\n"
	"in vec4 frag_color;\n"
	"\n"
	"uniform sampler2D diffuse_texture;\n"
	"out vec4 fragColor;\n"
	"%s" // SRGB_HELPER inserted here
	"void main() {\n"
	"    vec4 tex = texture(diffuse_texture, frag_uv);\n"
	"    if (tex.a < 0.01) discard;\n"
	"    vec3 lin_color = srgb_to_linear(frag_color.rgb);\n"
	"    fragColor = vec4(tex.rgb * lin_color, tex.a * frag_color.a);\n"
	"}";


// --------------------------------------------------------------------------

static shader_t* default_shader;
static shader_t* unlit_shader;
static shader_t* shadow_shader;
static shader_t* sprite_shader;


static int32_t get_uniform_location(shader_t* s, const char* uniform, uint32_t pos) {
	char buffer[64];
	sprintf(buffer, "light[%d].%s", pos, uniform);
	return glGetUniformLocation(s->program_id, buffer);
}


static void resolve_builtin_locations(shader_t* s) {
	s->locations.vp             = glGetUniformLocation(s->program_id, "vp");
	s->locations.surface_color  = glGetUniformLocation(s->program_id, "surface_color"); // -1 in Stage 23 lit/unlit
	s->locations.specular_color = glGetUniformLocation(s->program_id, "specular_color");
	s->locations.shininess      = glGetUniformLocation(s->program_id, "shininess");
	s->locations.emissive_color = glGetUniformLocation(s->program_id, "emissive_color");
	s->locations.opacity        = glGetUniformLocation(s->program_id, "opacity");

	for (uint32_t i = 0; i < MAX_LIGHTS; i++) {
		s->locations.light[i].light_type      = get_uniform_location(s, "light_type", i);
		s->locations.light[i].light_color     = get_uniform_location(s, "light_color", i);
		s->locations.light[i].light_intensity = get_uniform_location(s, "light_intensity", i);
		s->locations.light[i].light_direction = get_uniform_location(s, "light_direction", i);
		s->locations.light[i].light_position  = get_uniform_location(s, "light_position", i);
		s->locations.light[i].constant_attenuation  = get_uniform_location(s, "constant_attenuation", i);
		s->locations.light[i].linear_attenuation    = get_uniform_location(s, "linear_attenuation", i);
		s->locations.light[i].quadratic_attenuation = get_uniform_location(s, "quadratic_attenuation", i);
		s->locations.light[i].cone_inner_cutoff = get_uniform_location(s, "cone_inner_cutoff", i);
		s->locations.light[i].cone_outer_cutoff = get_uniform_location(s, "cone_outer_cutoff", i);
	}

	s->locations.light_count     = glGetUniformLocation(s->program_id, "light_count");
	s->locations.camera_position = glGetUniformLocation(s->program_id, "camera_position");
	s->locations.diffuse_texture = glGetUniformLocation(s->program_id, "diffuse_texture");
	s->locations.normal_texture  = glGetUniformLocation(s->program_id, "normal_texture");
	s->locations.ambient_factor  = glGetUniformLocation(s->program_id, "ambient_factor");

	s->locations.shadow_atlas = glGetUniformLocation(s->program_id, "shadow_atlas");
	for (uint32_t i = 0; i < MAX_LIGHTS; i++) {
		char shadow_rect[32];
		char light_vp[32];
		sprintf(shadow_rect, "shadow_rect[%d]", i);
		sprintf(light_vp,    "light_vp[%d]", i);
		s->locations.shadow_rect[i] = glGetUniformLocation(s->program_id, shadow_rect);
		s->locations.light_vp[i]    = glGetUniformLocation(s->program_id, light_vp);
	}
	for (uint32_t i = 0; i < MAX_CASCADES; i++) {
		char buf[32];
		sprintf(buf, "cascade_vp[%d]", i);
		s->locations.cascade_vp[i] = glGetUniformLocation(s->program_id, buf);
		sprintf(buf, "cascade_rect[%d]", i);
		s->locations.cascade_rect[i] = glGetUniformLocation(s->program_id, buf);
		sprintf(buf, "cascade_splits[%d]", i);
		s->locations.cascade_splits[i] = glGetUniformLocation(s->program_id, buf);
	}
	s->locations.cascade_count = glGetUniformLocation(s->program_id, "cascade_count");
	s->locations.view_matrix   = glGetUniformLocation(s->program_id, "view_matrix");
}


shader_t* shader_create(const char* vs, const char* fs) {
	uint32_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vs, NULL);
	glCompileShader(vertex_shader);
	int32_t success = 0;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char log[512];
		glGetShaderInfoLog(vertex_shader, 512, NULL, log);
		fprintf(stderr, "Vertex shader compilation error: %s\n", log);
		return NULL;
	}

	uint32_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fs, NULL);
	glCompileShader(fragment_shader);
	success = 0;
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char log[512];
		glGetShaderInfoLog(fragment_shader, 512, NULL, log);
		fprintf(stderr, "Fragment shader compilation error: %s\n", log);
		return NULL;
	}

	uint32_t shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);
	success = 0;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
	if (!success) {
		char log[512];
		glGetProgramInfoLog(shader_program, 512, NULL, log);
		fprintf(stderr, "Shader program link error: %s\n", log);
		return NULL;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	shader_t* s = malloc(sizeof(*s));
	if (!s) {
		fprintf(stderr, "Failed to allocate shader object\n");
		return NULL;
	}
	s->program_id    = shader_program;
	s->uniform_count = 0;
	resolve_builtin_locations(s);
	return s;
}


static int32_t search_location(char* name, shader_t* s) {
	glUseProgram(s->program_id);
	s->uniform_table[s->uniform_count].location = glGetUniformLocation(s->program_id, name);
	if (s->uniform_table[s->uniform_count].location >= 0) {
		s->uniform_table[s->uniform_count].name = malloc(strlen(name) + 1);
		if (!s->uniform_table[s->uniform_count].name) {
			fprintf(stderr, "search_location: failed to allocate uniform table entry\n");
			return -1;
		}
		strcpy(s->uniform_table[s->uniform_count].name, name);
		s->uniform_count++;
		return s->uniform_count - 1;
	}
	fprintf(stderr, "search_location: uniform cannot be found\n");
	return -1;
}


int32_t shader_uniform_lookup(shader_t* s, char* uniform_name) {
	if (!s) return false;
	for (uint32_t i = 0; i < s->uniform_count; i++) {
		if (strcmp(uniform_name, s->uniform_table[i].name) == 0) return i;
	}
	if (s->uniform_count < MAX_UNIFORMS - 1) return search_location(uniform_name, s);
	return -1;
}


void shader_destroy(shader_t* s) {
	if (s) {
		glDeleteProgram(s->program_id);
		for (uint32_t i = 0; i < s->uniform_count; i++) free(s->uniform_table[i].name);
		free(s);
	}
}


void shader_init(void) {
	// ---- Lit fragment shader: header + sRGB helper + body ----
	static char lit_fs_src[16384];
	int n = sprintf(lit_fs_src,
		"#version 330 core\n"
		"#define MAX_LIGHTS %d\n"
		"#define MAX_CASCADES %d\n"
		"#define LIGHT_OFF %d\n"
		"#define LIGHT_DIRECTIONAL %d\n"
		"#define LIGHT_POINT %d\n"
		"#define LIGHT_SPOT %d\n",
		MAX_LIGHTS, MAX_CASCADES,
		LIGHT_OFF, LIGHT_DIRECTIONAL, LIGHT_POINT, LIGHT_SPOT);
	strcat(lit_fs_src + n, SRGB_HELPER);
	strcat(lit_fs_src, default_fs_src);

	default_shader = shader_create(default_vs_src, lit_fs_src);
	if (!default_shader) { fprintf(stderr, "Failed to create lit shader\n"); exit(-1); }

	// ---- Unlit fragment: template + helper ----
	static char unlit_fs_src[2048];
	sprintf(unlit_fs_src, unlit_fs_template, SRGB_HELPER);
	unlit_shader = shader_create(unlit_vs_src, unlit_fs_src);
	if (!unlit_shader) { fprintf(stderr, "Failed to create unlit shader\n"); exit(-1); }

	// ---- Shadow: unchanged, no color work ----
	shadow_shader = shader_create(shadow_vs_src, shadow_fs_src);
	if (!shadow_shader) { fprintf(stderr, "Failed to create shadow shader\n"); exit(-1); }

	// ---- Sprite: template + helper ----
	static char sprite_fs_src[2048];
	sprintf(sprite_fs_src, sprite_fs_template, SRGB_HELPER);
	sprite_shader = shader_create(sprite_vs_src, sprite_fs_src);
	if (!sprite_shader) { fprintf(stderr, "Failed to create sprite shader\n"); exit(-1); }
}


void shader_shutdown(void) {
	shader_destroy(default_shader);
	shader_destroy(unlit_shader);
	shader_destroy(shadow_shader);
	shader_destroy(sprite_shader);
}


shader_t* shader_get_default(void) { return default_shader; }
shader_t* shader_get_unlit(void)   { return unlit_shader; }
shader_t* shader_get_shadow(void)  { return shadow_shader; }
shader_t* shader_get_sprite(void)  { return sprite_shader; }
