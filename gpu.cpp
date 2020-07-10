#include "gpu.h"
#include "misc.h"
#include "sys.h"
#include "pov.h"
#include "voxels.h"

#include <glm/vec2.hpp>
#include <iostream>
#include <filesystem>
#include <optional>
#include <map>
#include <string.h>
#include <assert.h>
#include <fmt/format.h>
#include <magic_enum.hpp>

namespace cw::gpu {

	bool enable_wireframe = false;
	GLuint primary_render_buffer = 0, primary_frame_buffer = 0;
	GLuint deferred_surface_render_target = 0, deferred_position_render_target = 0, deferred_material_render_target = 0;
	GLuint screen_quad_vertex_array = 0, screen_quad_vertex_buffer = 0;
	void print_program_info_log(GLuint id);
	GLuint make_program_from_shaders(const std::vector<GLuint> &shaders);
	void print_shader_info_log(GLuint id);
	void perform_shader_preprocessor(const std::filesystem::path &path, std::vector<char> &content);
	GLuint make_shader_from_file(const std::filesystem::path &path);
	std::optional<std::map<std::string, GLuint>> make_programs_from_directory(const std::filesystem::path &path);
	void make_screen_quad();
	void generate_render_targets();
	bool initialize();
	void shutdown();
	void render();
}

namespace cw::textures {

	extern GLuint voxel_array;
	extern GLuint x256_array;
	extern GLuint x512_array;

	extern std::map<std::string, int> voxel_indices;
	extern std::map<std::string, int> x256_indices;
	extern std::map<std::string, int> x512_indices;

	void load_all();
}

namespace cw::core {
	void on_deferred_render();
}

std::map<std::string, GLuint> cw::gpu::programs;
glm::ivec2 cw::gpu::render_target_size { 0, 0 };

void cw::gpu::print_program_info_log(GLuint id) {
	GLint log_length;
	glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_length);
	if (!log_length) {
		std::cout << "GL program info log is blank." << std::endl;
		return;
	}
	std::vector<char> log(log_length);
	glGetProgramInfoLog(id, log.size(), 0, log.data());
	std::cout << log.data() << std::endl;
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
		std::cout << "GL shader info log is blank." << std::endl;
		return;
	}
	std::vector<char> log(log_length);
	glGetShaderInfoLog(id, log.size(), 0, log.data());
	std::cout << log.data() << std::endl;
}

void cw::gpu::perform_shader_preprocessor(const std::filesystem::path &path, std::vector<char> &content) {
	const char *voxel_vti_constants = "{{{ VOXEL TEXTURE INDICES }}}";
	std::string copy_of(content.begin(), content.end());
	auto position = copy_of.find(voxel_vti_constants);
	if (position == std::string::npos) return;
	std::cout << "Writing runtime-generated VTI constants to shader: \"" << path.string() << "\"" << std::endl;
	std::string runtime_vti_reflect;
	constexpr auto voxel_names = magic_enum::enum_names<voxels::id>();
	constexpr std::size_t voxel_names_num = magic_enum::enum_count<voxels::id>();
	for (size_t i = 0; i < voxel_names_num; i++) {
		const auto name = static_cast<std::string>(voxel_names[i]);
		if (name == "null") continue;
		if (textures::voxel_indices.find(name) == textures::voxel_indices.end()) {
			std::cout << "No matching texture found for VTI: \"" << name << "\"" << std::endl;
			continue;
		}
		const auto new_line = fmt::format("const float vti_{} = {};", name, textures::voxel_indices[name]);
		runtime_vti_reflect += new_line + "\n";
		std::cout << " + " << new_line << std::endl;
	}
	copy_of.replace(position, strlen(voxel_vti_constants), runtime_vti_reflect);
	content = std::vector<char>(copy_of.begin(), copy_of.end());
}

GLuint cw::gpu::make_shader_from_file(const std::filesystem::path &path) {
	auto content = cw::misc::read_file(path);
	if (!content) return 0;
	perform_shader_preprocessor(path, *content);
	GLenum type;
	if (path.extension().string() == ".vs") type = GL_VERTEX_SHADER;
	else if (path.extension().string() == ".fs") type = GL_FRAGMENT_SHADER;
	else if (path.extension().string() == ".gs") type = GL_GEOMETRY_SHADER;
	else {
		std::cout << "File does not have a valid GL shader extension: " << path.string() << std::endl;
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
				std::cout << "Failed to compile." << std::endl;
				success = false;
				break;
			}
			std::cout << "GL shader: " << pair.first << "[" << extension << "] -> ";
			std::cout << "Compiled." << " (" << shader << ")" << std::endl;
			all_shaders.push_back(shader);
			shaders.push_back(shader);
		}
		if (!success) break;
		auto program = make_program_from_shaders(shaders);
		if (!program) {
			std::cout << "GL program: " << pair.first << " -> ";
			std::cout << "Failed to link." << std::endl;
			success = false;
			break;
		}
		std::cout << "GL program: " << pair.first << " -> ";
		std::cout << "Linked." << " (" << program << ")" << std::endl;
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
	std::cout << "All render targets are ready. (" << render_target_size.x << " by " << render_target_size.y << ") " << std::endl;
	GLenum fragment_buffers[] = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2
	};
	glDrawBuffers(3, fragment_buffers);
}

bool cw::gpu::initialize() {
	textures::load_all();
	auto result = make_programs_from_directory(sys::bin_path().string() + "glsl\\");
	if (!result) {
		std::cout << "Failed to create GPU programs." << std::endl;
		return false;
	}
	programs = *result;
	make_screen_quad();
	return true;
}

void cw::gpu::shutdown() {

}

void cw::gpu::render() {
	glBindFramebuffer(GL_FRAMEBUFFER, primary_frame_buffer);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, render_target_size.x, render_target_size.y);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	if (enable_wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth(2);
	} else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	core::on_deferred_render();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glViewport(0, 0, render_target_size.x, render_target_size.y);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glUseProgram(programs["screen"]);
	static float saturation_power = 1;
	static float gamma_power = 1.5;
	static bool show_render_buffers = false;
	GLint pixel_w_location = glGetUniformLocation(programs["screen"], "pixel_w");
	GLint pixel_h_location = glGetUniformLocation(programs["screen"], "pixel_h");
	GLint saturation_power_location = glGetUniformLocation(programs["screen"], "saturation_power");
	GLint gamma_power_location = glGetUniformLocation(programs["screen"], "gamma_power");
	GLint near_plane_location = glGetUniformLocation(programs["screen"], "near_plane");
	GLint far_plane_location = glGetUniformLocation(programs["screen"], "far_plane");
	if (pixel_w_location != -1) glUniform1f(pixel_w_location, 1.f / static_cast<float>(render_target_size.x));
	if (pixel_h_location != -1) glUniform1f(pixel_h_location, 1.f / static_cast<float>(render_target_size.y));
	if (saturation_power_location != -1) glUniform1f(saturation_power_location, saturation_power);
	if (gamma_power_location != -1) glUniform1f(gamma_power_location, gamma_power);
	if (near_plane_location != -1) glUniform1f(near_plane_location, pov::near_plane_distance);
	if (far_plane_location != -1) glUniform1f(far_plane_location, pov::far_plane_distance);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, deferred_surface_render_target);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, deferred_position_render_target);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, deferred_material_render_target);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures::voxel_array);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures::x256_array);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures::x512_array);
	glBindVertexArray(screen_quad_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, screen_quad_vertex_buffer);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}
