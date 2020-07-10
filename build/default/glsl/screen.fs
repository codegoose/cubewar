#version 440 core

layout (binding=0) uniform sampler2D deferred_surface_buffer;
layout (binding=1) uniform sampler2D deferred_position_buffer;
layout (binding=2) uniform sampler2D deferred_material_buffer;
layout (binding=3) uniform sampler2DArray voxel_array;
layout (binding=4) uniform sampler2DArray x256_array;
layout (binding=5) uniform sampler2DArray x512_array;

uniform float pixel_w;
uniform float pixel_h;
uniform float saturation_power;
uniform float gamma_power;
uniform float near_plane;
uniform float far_plane;

in vec2 sh_uv;
out vec4 final_color;

vec3 get_diffuse(vec2 uv) {
	vec4 material_coords = texture2D(deferred_material_buffer, uv);
	if (material_coords.a == 0) return vec3(0.4, 0.5, 1.0); // sky
	vec3 normal = texture2D(deferred_surface_buffer, uv).rgb;
	float light_power = max(dot(normal, normalize(vec3(0.1, 0.1, 0.8))) * 1.5, 0.82);
	if (material_coords.a == 1) return texture(voxel_array, material_coords.rgb).rgb * light_power; // voxel
	if (material_coords.a == 1000) return texture(x512_array, vec3(material_coords.rg, 0)).rgb * light_power; // mesh
	if (material_coords.a == 1001) return texture(x512_array, vec3(material_coords.rg, 1)).rgb * light_power; // mesh
	if (material_coords.a == 1002) return texture(x256_array, vec3(material_coords.rg, 1)).rgb * light_power; // mesh
}

float get_depth(vec2 uv) {
	vec4 material_coords = texture2D(deferred_material_buffer, uv);
	if (material_coords.a == 0) return far_plane;
	return texture2D(deferred_position_buffer, uv).a;
}

vec3 apply_gamma(vec3 color) {
	return pow(color.rgb, vec3(1.0 / gamma_power));
}

vec3 get_saturated(vec3 color, float outline_d) {
    return mix(vec3(dot(color, vec3(0.2125, 0.7154, 0.0721))), color, outline_d);
}

/*
vec3 get_sharpened(vec3 color, vec2 coords) {
  vec3 sum = vec3(0.0);
  sum += -1.0 * get_diffuse(coords + vec2(-pixel_w , 0.0));
  sum += -1.0 * get_diffuse(coords + vec2(0, -pixel_h));
  sum += 5.0 * color;
  sum += -1.0 * get_diffuse(coords + vec2(0, pixel_h));
  sum += -1.0 * get_diffuse(coords + vec2(pixel_w , 0));
  return sum;
}
*/

vec2 promo_outline_offset[4] = vec2[4](
	vec2(-1, 1),
	vec2(0, 1),
	vec2(1, 1),
	vec2(1, 0)
);

vec3 promo_outline(vec3 color) {
	float outline_d = 1.0;
	float z = get_depth(sh_uv);
	float total_z = 0.0;
	float max_z = 0;
	float sample_z1 = 0.0;
	float sample_z2 = 0.0;
	for (int i = 0; i < 4; i++){
		sample_z1 = get_depth(sh_uv.xy + vec2(pixel_w, pixel_h) * promo_outline_offset[i]);
		max_z = max(sample_z1, max_z);
		sample_z2 = get_depth(sh_uv.xy - vec2(pixel_w, pixel_h) * promo_outline_offset[i]);
		max_z = max(sample_z2, max_z);
		outline_d *= clamp(1.0 - ((sample_z1 + sample_z2) - z * 2.0) * 32.0 / z, 0.0, 1.0);
		total_z += sample_z1 + sample_z2;
	}
	float outline_a = 1.0 - clamp((z * 8.0 - total_z) * 64.0 / z, 0.0, 1.0) * clamp(1.0 - ((z * 8.0 - total_z) * 32.0 - 1.0) / z, 0.0, 1.0);
	float outline_b = clamp(1.0 + 8.0 * (z - max_z) / z, 0.0, 1.0);
	float outline_c = clamp(1.0 + 64.0 * (z - max_z) / z, 0.0, 1.0);
	float outline = (0.35 * (outline_a * outline_b) + 0.65) * (0.75 * (1.0 - outline_d) * outline_c + 1.0);
	color = sqrt(sqrt(color));
	color *= outline;
	color *= color;
	color *= color;
	return color;
}

void main() {
	final_color = vec4(get_diffuse(sh_uv), 1);
	// final_color = vec4(promo_outline(final_color.rgb), final_color.a);
	final_color = vec4(apply_gamma(final_color.rgb), 1);
}