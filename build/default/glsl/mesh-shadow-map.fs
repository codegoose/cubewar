#version 440 core

layout (location=0) out vec2 out_sun_shadow_depth_map;
uniform vec2 material_identifier;
in float sh_depth;

void main() {
	out_sun_shadow_depth_map = vec2((sh_depth + 1.0) * 0.5, material_identifier.y);
}
