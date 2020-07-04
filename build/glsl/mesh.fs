#version 440 core

in float sh_depth;
in vec3 sh_world_position;
in vec3 sh_world_normal;
in vec2 sh_uv;

layout (location=0) out vec3 out_deferred_surface;
layout (location=1) out vec4 out_deferred_position;
layout (location=2) out vec4 out_deferred_material;

uniform vec2 material_identifier;

void main() {
	out_deferred_surface = sh_world_normal;
	out_deferred_position = vec4(sh_world_position, sh_depth);
	out_deferred_material = vec4(sh_uv, material_identifier);
}