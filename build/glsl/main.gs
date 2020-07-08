#version 440 core

layout (points) in;
layout (triangle_strip, max_vertices = 64) out;

uniform mat4 world_transform;
uniform mat4 total_transform;

flat in float encoded_voxel_faces[];

out float sh_depth;
out vec3 sh_world_position;
out vec3 sh_world_normal;

out geometry_data {
	vec3 normal;
	vec3 texture_coords;
} geometry;

void prepare_next_stage_info(vec3 vertex_location) {
	vec4 this_vertex = gl_in[0].gl_Position + vec4(vertex_location, 0);
	vec4 screenspace_coordinates = total_transform * this_vertex;
	gl_Position = screenspace_coordinates;
	sh_depth = screenspace_coordinates.z;
	sh_world_position = vec3(world_transform * this_vertex);
	sh_world_normal = normalize(vec3(world_transform * vec4(geometry.normal, 0)));
}

void main() {
	float working_encoded_voxel_faces = encoded_voxel_faces[0];
	bool enable_top_face = working_encoded_voxel_faces / 32 >= 1;
	if (enable_top_face) working_encoded_voxel_faces -= 32;
	bool enable_bottom_face = working_encoded_voxel_faces / 16 >= 1;
	if (enable_bottom_face) working_encoded_voxel_faces -= 16;
	bool enable_right_face = working_encoded_voxel_faces / 8 >= 1;
	if (enable_right_face) working_encoded_voxel_faces -= 8;
	bool enable_left_face = working_encoded_voxel_faces / 4 >= 1;
	if (enable_left_face) working_encoded_voxel_faces -= 4;
	bool enable_front_face = working_encoded_voxel_faces / 2 >= 1;
	if (enable_front_face) working_encoded_voxel_faces -= 2;
	bool enable_back_face = working_encoded_voxel_faces > 0;
	if (enable_top_face) {
		geometry.normal = vec3(0, 0, 1);
		geometry.texture_coords.z = 1;
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		geometry.texture_coords.x = 0;
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, .5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, .5));
		geometry.texture_coords.x = 0;
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		geometry.texture_coords.x = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, .5));
		geometry.texture_coords.y = 0;
		EmitVertex();
		EndPrimitive();
	}
	if (enable_bottom_face) {
		geometry.normal = vec3(0, 0, -1);
		geometry.texture_coords.z = 1;
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		geometry.texture_coords.x = 0;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		geometry.texture_coords.x = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		geometry.texture_coords.x = 0;
		geometry.texture_coords.y = 1;
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, -.5));
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		geometry.texture_coords.x = 0;
		EmitVertex();
		EndPrimitive();
	}
	if (enable_right_face) {
		geometry.normal = vec3(1, 0, 0);
		geometry.texture_coords.z = 1;
		prepare_next_stage_info(vec3(.5, .5, .5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		geometry.texture_coords.x = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 1;
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		geometry.texture_coords.x = 0;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, -.5));
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		geometry.texture_coords.x = 1;
		EmitVertex();
		EndPrimitive();
	}
	if (enable_left_face) {
		geometry.normal = vec3(-1, 0, 0);
		geometry.texture_coords.z = 1;
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, .5));
		geometry.texture_coords.x = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 1;
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(-.5, .5, .5));
		geometry.texture_coords.x = 0;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		geometry.texture_coords.x = 1;
		EmitVertex();
		EndPrimitive();
	}
	if (enable_front_face) {
		geometry.normal = vec3(0, 1, 0);
		geometry.texture_coords.z = 1;
		prepare_next_stage_info(vec3(-.5, .5, .5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, .5));
		geometry.texture_coords.x = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 1;
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(.5, .5, .5));
		geometry.texture_coords.x = 0;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		geometry.texture_coords.x = 1;
		EmitVertex();
		EndPrimitive();
	}
	if (enable_back_face) {
		geometry.normal = vec3(0, -1, 0);
		geometry.texture_coords.z = 1;
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		geometry.texture_coords.x = 0;
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		geometry.texture_coords.x = 1;
		geometry.texture_coords.y = 0;
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		geometry.texture_coords.x = 0;
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		geometry.texture_coords.y = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, -.5));
		geometry.texture_coords.x = 1;
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		geometry.texture_coords.y = 0;
		EmitVertex();
		EndPrimitive();
	}
}