#include <rang.hpp>
#include "gpu.h"
#include "misc.h"
#include <iostream>
#include <filesystem>
#include <optional>
#define GLEW_STATIC
#include <gl/glew.h>
#include <assert.h>

namespace cw::gpu {
	std::map<std::string, GLuint> programs;
	GLuint primary_render_buffer = 0, primary_frame_buffer = 0;
	GLuint deferred_surface_render_target = 0, deferred_position_render_target = 0, deferred_material_render_target = 0;
	GLuint screen_quad_vertex_array = 0, screen_quad_vertex_buffer = 0;
	void print_program_info_log(GLuint id);
	GLuint make_program_from_shaders(const std::vector<GLuint> &shaders);
	void print_shader_info_log(GLuint id);
	GLuint make_shader_from_file(const std::filesystem::path &path);
	std::optional<std::map<std::string, GLuint>> make_programs_from_directory(const std::filesystem::path &path);
	void make_screen_quad();
	void generate_render_targets();
	bool initialize();
	void shutdown();
}

glm::ivec2 cw::gpu::render_target_size { 0, 0 };

void cw::gpu::print_program_info_log(GLuint id) {
	GLint log_length;
	glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_length);
	if (!log_length) {
		std::cout << rang::fg::yellow << "GL program info log is blank." << rang::fg::reset << std::endl;
		return;
	}
	std::vector<char> log(log_length);
	glGetProgramInfoLog(id, log.size(), 0, log.data());
	std::cout << rang::fg::red << log.data() << rang::fg::reset << std::endl;
}

GLuint cw::gpu::make_program_from_shaders(const std::vector<GLuint> &shaders) {
	GLuint id = glCreateProgram();
	assert(id);
	for (auto &shader : shaders) glAttachShader(id, shader);
	glLinkProgram(id);
	GLint success;
	glGetProgramiv(id, GL_LINK_STATUS, &success);
	if (!success) {
		print_program_info_log(id);
		for (auto &shader : shaders) glDetachShader(id, shader);
		glDeleteProgram(id);
		return 0;
	}
	return id;
}

void cw::gpu::print_shader_info_log(GLuint id) {
	GLint log_length;
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);
	if (!log_length) {
		std::cout << rang::fg::yellow << "GL shader info log is blank." << rang::fg::reset << std::endl;
		return;
	}
	std::vector<char> log(log_length);
	glGetShaderInfoLog(id, log.size(), 0, log.data());
	std::cout << rang::fg::red << log.data() << rang::fg::reset << std::endl;
}

GLuint cw::gpu::make_shader_from_file(const std::filesystem::path &path) {
	auto content = cw::misc::read_file(path);
	if (!content) return 0;
	GLenum type;
	if (path.extension().string() == ".vs") type = GL_VERTEX_SHADER;
	else if (path.extension().string() == ".fs") type = GL_FRAGMENT_SHADER;
	else if (path.extension().string() == ".gs") type = GL_GEOMETRY_SHADER;
	else {
		std::cout << rang::fg::yellow << "File does not have a valid GL shader extension: " << path.string() << rang::fg::reset << std::endl;
		return 0;
	}
	GLuint id = glCreateShader(type);
	assert(id);
	const GLchar *const pointer = content->data();
	GLint content_length = content->size();
	glShaderSource(id, 1, &pointer, &content_length);
	glCompileShader(id);
	GLint success;
	glGetShaderiv(id, GL_COMPILE_STATUS, &success);
	if (!success) {
		print_shader_info_log(id);
		glDeleteShader(id);
		return 0;
	}
	return id;
}

std::optional<std::map<std::string, GLuint>> cw::gpu::make_programs_from_directory(const std::filesystem::path &path) {
	auto map = cw::misc::map_file_names_and_extensions(path);
	if (!map) return std::nullopt;
	std::map<std::string, GLuint> programs;
	std::vector<GLuint> all_shaders;
	bool success = true;
	for (auto &pair : *map) {
		std::vector<GLuint> shaders;
		for (auto &extension : pair.second) {
			auto shader = make_shader_from_file(path.string() + pair.first + extension);
			if (!shader) {
				std::cout << "GL shader: " << pair.first << "[" << extension << "] -> ";
				std::cout << rang::fg::red << "Failed to compile." << rang::fg::reset << std::endl;
				success = false;
				break;
			}
			std::cout << "GL shader: " << pair.first << "[" << extension << "] -> ";
			std::cout << rang::fg::green << "Compiled." << rang::fg::reset << " (" << shader << ")" << std::endl;
			all_shaders.push_back(shader);
			shaders.push_back(shader);
		}
		if (!success) break;
		auto program = make_program_from_shaders(shaders);
		if (!program) {
			std::cout << "GL program: " << pair.first << " -> ";
			std::cout << rang::fg::red << "Failed to link." << rang::fg::reset << std::endl;
			success = false;
			break;
		}
		std::cout << "GL program: " << pair.first << " -> ";
		std::cout << rang::fg::green << "Linked." << rang::fg::reset << " (" << program << ")" << std::endl;
		programs[pair.first] = program;
	}
	for (auto &shader : all_shaders) glDeleteShader(shader);
	if (!success) {
		for (auto &pair : programs) glDeleteProgram(pair.second);
		return std::nullopt;
	}
	return programs;
}

void cw::gpu::make_screen_quad() {
	const float points[] = {
		-1, 1, 0, 1,
		1, 1, 1, 1,
		-1, -1, 0, 0,
		1, 1, 1, 1,
		-1, -1, 0, 0,
		1, -1, 1, 0,
	};
	glGenVertexArrays(1, &screen_quad_vertex_array);
	glGenBuffers(1, &screen_quad_vertex_buffer);
	glBindVertexArray(screen_quad_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, screen_quad_vertex_buffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void *>(sizeof(float) * 2));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
}

void cw::gpu::generate_render_targets() {
	if (!deferred_surface_render_target) {
		glGenTextures(1, &deferred_surface_render_target);
		assert(deferred_surface_render_target);
		std::cout << "Generated texture for deferred surface render target. (#" << deferred_surface_render_target << ")" << std::endl;
	}
	if (!deferred_position_render_target) {
		glGenTextures(1, &deferred_position_render_target);
		assert(deferred_position_render_target);
		std::cout << "Generated texture for deferred position render target. (#" << deferred_position_render_target << ")" << std::endl;
	}
	if (!deferred_material_render_target) {
		glGenTextures(1, &deferred_material_render_target);
		assert(deferred_material_render_target);
		std::cout << "Generated texture for deferred material render target. (#" << deferred_material_render_target << ")" << std::endl;
	}
	glBindTexture(GL_TEXTURE_2D, deferred_surface_render_target);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, render_target_size.x, render_target_size.y, 0, GL_RGB, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, deferred_position_render_target);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, render_target_size.x, render_target_size.y, 0, GL_RGBA, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, deferred_material_render_target);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, render_target_size.x, render_target_size.y, 0, GL_RGBA, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (!primary_frame_buffer) {
		glGenFramebuffers(1, &primary_frame_buffer);
		assert(primary_frame_buffer);
		std::cout << "Generated primary frame buffer for render target. (#" << primary_frame_buffer << ")" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, primary_frame_buffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, deferred_surface_render_target, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, deferred_position_render_target, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, deferred_material_render_target, 0);
	if (!primary_render_buffer) {
		glGenRenderbuffers(1, &primary_render_buffer);
		assert(primary_render_buffer);
		std::cout << "Generated primary render buffer for render target. (#" << primary_render_buffer << ")" << std::endl;
	}
	glBindRenderbuffer(GL_RENDERBUFFER, primary_render_buffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, render_target_size.x, render_target_size.y);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, primary_render_buffer);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	std::cout << rang::fg::cyan << "All render targets are ready. (" << render_target_size.x << " by " << render_target_size.y << ") " << rang::fg::reset << std::endl;
	GLenum fragment_buffers[] = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2
	};
	glDrawBuffers(3, fragment_buffers);
}

bool cw::gpu::initialize() {
	auto result = make_programs_from_directory("glsl/");
	if (!result) {
		std::cout << rang::fg::red << "Failed to create GPU programs." << rang::fg::reset << std::endl;
		return false;
	}
	programs = *result;
	return true;
}

void cw::gpu::shutdown() {

}