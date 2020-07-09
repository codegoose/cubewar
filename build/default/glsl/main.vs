#version 440 core

layout (location=0) in vec3 position;
layout (location=1) in float meta;

flat out float encoded_voxel_faces;

void main() {
	encoded_voxel_faces = meta;
	gl_Position = vec4(position.x, position.y, position.z, 1);
}