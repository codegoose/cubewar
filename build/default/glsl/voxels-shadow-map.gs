#version 440 core

layout (points) in;
layout (triangle_strip, max_vertices = 64) out;

uniform mat4 world_transform;
uniform mat4 total_transform;

flat in float encoded_voxel_faces[];

out float sh_depth;

void prepare_next_stage_info(vec3 vertex_location) {
	vertex_location *= 4.0f;
	vec4 this_vertex = gl_in[0].gl_Position + vec4(vertex_location, 0);
	vec4 screenspace_coordinates = total_transform * this_vertex;
	gl_Position = screenspace_coordinates;
	sh_depth = screenspace_coordinates.z;
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
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, .5));
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, .5));
		EmitVertex();
		EndPrimitive();
	}
	if (enable_bottom_face) {
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		EmitVertex();
		EndPrimitive();
	}
	if (enable_right_face) {
		prepare_next_stage_info(vec3(.5, .5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		EmitVertex();
		EndPrimitive();
	}
	if (enable_left_face) {
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(-.5, .5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		EmitVertex();
		EndPrimitive();
	}
	if (enable_front_face) {
		prepare_next_stage_info(vec3(-.5, .5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(.5, .5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, .5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, .5, -.5));
		EmitVertex();
		EndPrimitive();
	}
	if (enable_back_face) {
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		EmitVertex();
		prepare_next_stage_info(vec3(-.5, -.5, .5));
		EmitVertex();
		EndPrimitive();
		prepare_next_stage_info(vec3(-.5, -.5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, -.5));
		EmitVertex();
		prepare_next_stage_info(vec3(.5, -.5, .5));
		EmitVertex();
		EndPrimitive();
	}
}