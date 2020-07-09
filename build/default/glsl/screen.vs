#version 440 core

layout (location=0) in vec2 in_position;
layout (location=1) in vec2 in_uv;

varying out vec2 sh_uv;

void main() {
	gl_Position = vec4(in_position.x, in_position.y, 0, 1);
	sh_uv = in_uv;
}