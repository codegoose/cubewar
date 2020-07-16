#version 440 core

uniform mat4 world_transform;
uniform mat4 total_transform;

layout (location=0) in vec3 in_position;
layout (location=1) in vec3 in_normal;
layout (location=2) in vec2 in_uv;

out float sh_depth;

void main() {
	vec4 screenspace_coordinates = total_transform * vec4(in_position, 1);
	gl_Position = screenspace_coordinates;
	sh_depth = screenspace_coordinates.z;
}
