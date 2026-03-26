// shader.c

#include "shader.h"
#include "stb_math3d.h"

#include <glad/glad.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Lit shader sources — replace the existing default_vs_src and default_fs_src in shader.c

static const char* default_vs_src = "#version 330 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 1) in vec3 aNormal;\n"
	"layout (location = 2) in vec2 aUV;\n"
	"layout (location = 3) in vec3 aTangent;\n"
	"layout (location = 6) in mat4 aModel;\n"
	"layout (location =10) in vec4 aUV_rect;\n"
	"uniform mat4 vp;\n"
	"out vec3 frag_position;\n"
	"out vec3 frag_normal;\n"
	"out vec3 frag_tangent;\n"
	"out vec3 frag_bitangent;\n"
	"out vec2 frag_uv;\n"
	"void main() {\n"
	"	vec4 world_pos = aModel * vec4(aPos, 1.0);\n"
	"	frag_position = world_pos.xyz;\n"
	"	frag_normal = normalize(mat3(aModel) * aNormal);\n"
	"	frag_tangent = mat3(aModel) * aTangent;\n"
	"	frag_bitangent = cross(frag_normal, frag_tangent);\n"
	"	frag_uv = aUV * (aUV_rect.zw - aUV_rect.xy) + aUV_rect.xy;\n"
	"	gl_Position = vp * world_pos;\n"
	"}\0";

static const char* default_fs_src = "#version 330 core\n"
	"#define MAX_LIGHTS 16\n"
	"#define LIGHT_OFF 0\n"
	"#define LIGHT_DIRECTIONAL 1\n"
	"#define LIGHT_POINT 2\n"
	"#define LIGHT_SPOT 3\n"
	"\n"
	"struct Light {\n"
	"	int light_type;\n"
	"	vec3 light_color;\n"
	"	float light_intensity;\n"
	"	vec3 light_direction;\n"
	"	vec3 light_position;\n"
	"	float constant_attenuation;\n"
	"	float linear_attenuation;\n"
	"	float quadratic_attenuation;\n"
	"	float cone_inner_cutoff;\n"
	"	float cone_outer_cutoff;\n"
    "   int shadow_index;\n"            // -1 no shadow 0 to 3 othewise
	"};\n"
	"\n"
	"uniform Light light[MAX_LIGHTS];\n"
	"uniform int light_count;\n"
	"uniform float ambient_factor;\n"
	"uniform vec3 camera_position;\n"
	"\n"
	"uniform vec3 surface_color;\n"
	"uniform vec3 specular_color;\n"
	"uniform float shininess;\n"
	"uniform vec3 emissive_color;\n"
	"uniform sampler2D diffuse_texture;\n"
	"uniform sampler2D normal_texture;\n"
	"uniform sampler2D shadow_map[4];\n"
	"uniform mat4 light_vp[4];\n"
	"\n"
	"in vec3 frag_position;\n"
	"in vec3 frag_normal;\n"
	"in vec3 frag_tangent;\n"
	"in vec3 frag_bitangent;\n"
	"in vec2 frag_uv;\n"
	"out vec4 fragColor;\n"
	"\n"
	"float calculate_shadow(int pos) {\n"
    "	vec4 light_space_pos = light_vp[pos] * vec4(frag_position, 1.0);\n"
    "	vec3 proj = light_space_pos.xyz / light_space_pos.w;\n"
    "	proj = proj * 0.5 + 0.5;\n"
	"\n"
    "	if (proj.z > 1.0) return 0.0;\n"
	"\n"
    "	float bias = 0.005;\n"
    "	float shadow = 0.0;\n"
    "	vec2 texel_size = vec2(1.0) / vec2(textureSize(shadow_map[pos], 0));\n"
    "	for (int x = -2; x <= 2; x++) {\n"
    "	    for (int y = -2; y <= 2; y++) {\n"
    "	        float depth = texture(shadow_map[pos], proj.xy + vec2(x, y) * texel_size).r;\n"
    "	        shadow += proj.z - bias > depth ? 1.0 : 0.0;\n"
    "	    }\n"
    "	}\n"
    "	return shadow / 25.0;\n"
	"}\n"
	"\n"
	"void main() {\n"
	"   vec3 normal;\n"
	"	if (length(frag_tangent) > 0.001) {\n"
	"		mat3 TBN = mat3(frag_tangent, frag_bitangent, frag_normal);\n"
	"		vec3 map_normal = texture(normal_texture, frag_uv).rgb *2.0 - 1.0;\n"
	"		normal = normalize(TBN * map_normal);\n"
	"	} else {\n"
	"		normal = normalize(frag_normal);\n"
	"	}\n"
	"	vec3 view_dir = normalize(camera_position - frag_position);\n"
	"	vec3 tex_color = texture(diffuse_texture, frag_uv).rgb;\n"
	"   vec3 base_color = surface_color * tex_color;\n"
   	"\n"
	"	vec3 result = vec3(0.0);\n"
	"\n"
	"	for (int i = 0; i < light_count; i++) {\n"
	"		if (light[i].light_type == LIGHT_OFF) continue;\n"
	"\n"
	"		vec3 light_dir;\n"
	"		float attenuation = 1.0;\n"
	"		float spot_factor = 1.0;\n"
	"\n"
	"		if (light[i].light_type == LIGHT_DIRECTIONAL) {\n"
	"			light_dir = normalize(-light[i].light_direction);\n"
	"		} else {\n"
	"			vec3 to_light = light[i].light_position - frag_position;\n"
	"			float distance = length(to_light);\n"
	"			light_dir = to_light / distance;\n"
	"			attenuation = 1.0 / (light[i].constant_attenuation\n"
	"				+ light[i].linear_attenuation * distance\n"
	"				+ light[i].quadratic_attenuation * distance * distance);\n"
	"\n"
	"			if (light[i].light_type == LIGHT_SPOT) {\n"
	"				float theta = dot(light_dir, normalize(-light[i].light_direction));\n"
	"				float inner = cos(light[i].cone_inner_cutoff);\n"
	"				float outer = cos(light[i].cone_outer_cutoff);\n"
	"				spot_factor = clamp((theta - outer) / (inner - outer), 0.0, 1.0);\n"
	"			}\n"
	"		}\n"
	"\n"
	"		float diff = max(dot(normal, light_dir), 0.0);\n"
	"		vec3 half_dir = normalize(light_dir + view_dir);\n"
	"		float spec = pow(max(dot(normal, half_dir), 0.0), shininess);\n"
	"		if (diff <= 0.0) spec = 0.0;\n"
	"\n"
	"		vec3 light_contrib = light[i].light_color * light[i].light_intensity;\n"
	"		vec3 ambient = ambient_factor * light_contrib * base_color;\n"
	"		vec3 diffuse = diff * light_contrib * base_color;\n"
	"		vec3 specular = spec * light_contrib * specular_color;\n"
	"\n"
    "       float shadow_factor = 1.0;\n"
    "       if (light[i].shadow_index >= 0)\n"
    "            shadow_factor = (1.0 - calculate_shadow(light[i].shadow_index));\n"
	"		result += ambient + (diffuse + specular) * attenuation * spot_factor * shadow_factor;\n"
	"	}\n"
	"\n"
	"	result += emissive_color;\n"
	"\n"
	"	if (light_count == 0) {\n"
	"		result = base_color;\n"
	"	}\n"
	"\n"
	"	fragColor = vec4(result, 1.0);\n"
	"}\0";



static const char* unlit_vs_src = "#version 330 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 6) in mat4 aModel;\n"
	"uniform mat4 vp;\n"
	"void main() {\n"
	"	gl_Position = vp * aModel * vec4(aPos, 1.0);\n"
	"}\0";


static const char* unlit_fs_src = "#version 330\n"
	"out vec4 fragColor;\n"
	"uniform vec3 surface_color;\n"
	"void main() {\n"
	"	fragColor = vec4(surface_color, 1.0f);\n"
	"}\0";



static const char* shadow_vs_src = "#version 330 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 6) in mat4 aModel;\n"
	"uniform mat4 vp;\n"
	"void main() {\n"
	"	gl_Position = vp * aModel * vec4(aPos, 1.0);\n"
	"}\0";


static const char* shadow_fs_src = "#version 330\n"
	"void main() {\n"
	"	;\n"
	"}\0";




static shader_t* default_shader;
static shader_t* unlit_shader;
static shader_t* shadow_shader;


static int32_t get_uniform_location(shader_t* s, const char* uniform, uint32_t pos) {
	char buffer[32];
	sprintf(buffer, "light[%d].%s", pos, uniform);
	return glGetUniformLocation(s->program_id, buffer);
}

/*
 * Resolve all known built-in uniform names against a compiled
 * shader program. Returns -1 for any uniform not found in the
 * shader — the renderer skips those at draw time.
 */
static void resolve_builtin_locations(shader_t* s) {
	// Material locations:
	s->locations.vp            = glGetUniformLocation(s->program_id, "vp");
	s->locations.surface_color = glGetUniformLocation(s->program_id, "surface_color");
	s->locations.specular_color   = glGetUniformLocation(s->program_id, "specular_color");
	s->locations.shininess        = glGetUniformLocation(s->program_id, "shininess");
	s->locations.emissive_color   = glGetUniformLocation(s->program_id, "emissive_color");

	// Lighting locations:
	for (uint32_t i = 0; i < MAX_LIGHTS; i++) {
		s->locations.light[i].light_type      = get_uniform_location(s, "light_type", i);
		s->locations.light[i].light_color     = get_uniform_location(s, "light_color", i);
		s->locations.light[i].light_intensity = get_uniform_location(s, "light_intensity", i);
		s->locations.light[i].light_direction = get_uniform_location(s, "light_direction", i);
		s->locations.light[i].light_position = get_uniform_location(s, "light_position", i);
		s->locations.light[i].constant_attenuation = get_uniform_location(s, "constant_attenuation", i);
		s->locations.light[i].linear_attenuation = get_uniform_location(s, "linear_attenuation", i);
		s->locations.light[i].quadratic_attenuation = get_uniform_location(s, "quadratic_attenuation", i);
		s->locations.light[i].cone_inner_cutoff = get_uniform_location(s, "cone_inner_cutoff", i);
		s->locations.light[i].cone_outer_cutoff = get_uniform_location(s, "cone_outer_cutoff", i);
        s->locations.light[i].shadow_index = get_uniform_location(s, "shadow_index", i);
	}

 	s->locations.light_count  = glGetUniformLocation(s->program_id, "light_count");
	s->locations.camera_position  = glGetUniformLocation(s->program_id, "camera_position");

	// Texture
	s->locations.diffuse_texture = glGetUniformLocation(s->program_id, "diffuse_texture");
	s->locations.normal_texture = glGetUniformLocation(s->program_id, "normal_texture");

	// Ambient related
	s->locations.ambient_factor = glGetUniformLocation(s->program_id, "ambient_factor");

	// Shadow map
    for (uint32_t i = 0; i < MAX_SHADOW_MAPS; i++) {
        char shadow_map[32];
        char light_vp[32];
	    sprintf(shadow_map, "shadow_map[%d]", i);
	    sprintf(light_vp, "light_vp[%d]", i);
	    s->locations.shadow_map[i] = glGetUniformLocation(s->program_id, shadow_map);
	    s->locations.light_vp[i] = glGetUniformLocation(s->program_id, light_vp);
    }
}




shader_t *shader_create(const char* vs, const char* fs) {
	uint32_t vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vs, NULL);
	glCompileShader(vertex_shader);
	int32_t success =  0;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success){
		char log[512];
		glGetShaderInfoLog(vertex_shader, 512, NULL, log);
		fprintf(stderr, "Vertex shader compilation error :%s\n", log);
		return NULL;
	}

	uint32_t fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fs, NULL);
	glCompileShader(fragment_shader);
	success =  0;
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success){
		char log[512];
		glGetShaderInfoLog(fragment_shader, 512, NULL, log);
		fprintf(stderr, "Fragment shader compilation error :%s\n", log);
		return NULL;
	}

	uint32_t shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);
	success =  0;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
	if (!success){
		char log[512];
		glGetProgramInfoLog(shader_program, 512, NULL, log);
		fprintf(stderr, "Shader program link error :%s\n", log);
		return NULL;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	shader_t *s = malloc(sizeof(*s));
	if (s == NULL) {
		fprintf(stderr, "Failed to allocate shader object\n");
		return NULL;
	}
	s->program_id = shader_program;
	s->uniform_count = 0;

	resolve_builtin_locations(s);

 	return s;
}

/* This internal function search for a location in the shader program
 * passed and if it found it updates the uniform_table, update the
 * handle parameter  and return true. False otherwise.
 * TODO: now the name is not binary safe will need to add a binary safe
 * string implementation */
static int32_t  search_location(char* name, shader_t* s) {
	glUseProgram(s->program_id);
	s->uniform_table[s->uniform_count].location = glGetUniformLocation(s->program_id, name);

	if (s->uniform_table[s->uniform_count].location >= 0) {
		s->uniform_table[s->uniform_count].name = malloc(strlen(name)+1);
		if (s->uniform_table[s->uniform_count].name == NULL) {
			fprintf(stderr, "search_location: failed to allocate uniform table entry\n");
			goto ERR;
		}
		strcpy(s->uniform_table[s->uniform_count].name, name);
 		s->uniform_count ++;
		return s->uniform_count-1;
	}

	fprintf(stderr, "search_location: uniform cannot be found\n");
ERR:
	return -1;
}

/* This function lookup for an uniform inside the shader's uniform table and
 * return the uniform_handle (the position in the table) tha can be used to
 * access the uniform location. If the name is not found in the table then the
 * serch goes in to the shader it self and if the unifomr name is found a new
 * entry in the uniform_table will be created ane the position returned. If the
 * unform name is not find in the table or the shader then the function return
 * -1 */
int32_t shader_uniform_lookup(shader_t* s, char* uniform_name) {
	if (!s) return false;
	for (uint32_t i = 0; i < s->uniform_count; i++) {
		if (strcmp(uniform_name, s->uniform_table[i].name) == 0) {
			return i;
		}
	}

	if (s->uniform_count < MAX_UNIFORMS-1) return search_location(uniform_name, s);

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
    default_shader = shader_create(default_vs_src, default_fs_src);
	if (default_shader == NULL) {
		fprintf(stderr, "Failed to create default shader\n");
		exit(-1);
	}

	unlit_shader = shader_create(unlit_vs_src, unlit_fs_src);
	if (unlit_shader == NULL) {
		fprintf(stderr, "Failed to create default unlit shader\n");
		exit(-1);
	}

 	shadow_shader = shader_create(shadow_vs_src, shadow_fs_src);
	if (shadow_shader == NULL) {
		fprintf(stderr, "Failed to create default shadow shader\n");
		exit(-1);
	}

}

void shader_shutdown(void) {
	shader_destroy(default_shader);
	shader_destroy(unlit_shader);
	shader_destroy(shadow_shader);
}

shader_t* shader_get_default(void) {
	return default_shader;
}


shader_t* shader_get_unlit(void) {
	return unlit_shader;
}

shader_t* shader_get_shadow(void) {
	return shadow_shader;
}

