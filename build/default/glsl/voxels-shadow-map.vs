#version 440 core

layout (location=0) in vec3 position;
layout (location=1) in float meta;
layout (location=2) in float matid;

flat out float encoded_voxel_faces;
flat out float voxel_material_id;

void main() {
	encoded_voxel_faces = meta;
	voxel_material_id = matid;
	gl_Position = vec4(position.x, position.y, position.z, 1);
}
