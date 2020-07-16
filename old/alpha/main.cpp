#define WIN32_LEAN_AND_MEAN
#include <rang.hpp>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <btBulletDynamicsCommon.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>
#include <filesystem>
#include <thread>
#define CG_EXPOSE_EVERYTHING
#include "subsystem.h"
#undef min
#undef max
#undef near
#undef far

#include "shadernaut.h"

namespace physics {

	btDefaultCollisionConfiguration *configuration = 0;
	btCollisionDispatcher *dispatcher = 0;
	btBroadphaseInterface *overlapping_pair_cache = 0;
	btSequentialImpulseConstraintSolver *solver = 0;
	btDiscreteDynamicsWorld *dynamics_world = 0;

	void cleanup() {
		if (dynamics_world) delete dynamics_world;
		if (solver) delete solver;
		if (overlapping_pair_cache) delete overlapping_pair_cache;
		if (dispatcher) delete dispatcher;
		if (configuration) delete configuration;
		dynamics_world = 0;
		solver = 0;
		overlapping_pair_cache = 0;
		dispatcher = 0;
		configuration = 0;
	}

	void initialize() {
		cleanup();
		configuration = new btDefaultCollisionConfiguration;
		dispatcher = new btCollisionDispatcher(configuration);
		overlapping_pair_cache = new btDbvtBroadphase;
		solver = new btSequentialImpulseConstraintSolver;
		dynamics_world = new btDiscreteDynamicsWorld(dispatcher, overlapping_pair_cache, solver, configuration);
		dynamics_world->setGravity({ 0, -10, 0 });
	}

	glm::vec3 from_bullet(const btVector3 &in) {
		return { in.x(), -in.z(), in.y() };
	}

	btVector3 to_bullet(const glm::vec3 &in) {
		return { in.x, in.z, -in.y };
	}

	glm::quat from_bullet(const btQuaternion &in) {
		glm::quat q;
		q.w = in.w();
		q.x = in.x();
		q.y = -in.z();
		q.z = in.y();
		return q;
	}

	btQuaternion to_bullet(const glm::quat &in) {
		btQuaternion q;
		q.setW(in.x);
		q.setX(in.z);
		q.setY(-in.y);
		q.setZ(in.w);
		return q;
	}
}

namespace pov {

	glm::vec3 eye { 0, 0, 0 } , center { 0, 0, 0 }, look { 0, 0, 0 };
	glm::quat orientation { glm::identity<glm::quat>() };
	float vertical_fov = 1.39626;
	float near = 0.01, far = 100.01;
	glm::mat4 view_matrix { glm::identity<glm::mat4>() };
	const glm::vec3 up { 0, 0, 1 };
}

struct geometry_point {
	GLfloat x, y, z, meta;
};

std::optional<std::vector<char>> read_file(const std::filesystem::path &path) {
	std::ifstream in(path.string(), std::ios::binary);
	if (!in.is_open()) {
		std::cout << "[textures] " << rang::fg::yellow << "Failed to open file for reading: " << path.string() << rang::fg::reset << std::endl;
		return std::nullopt;
	}
	std::vector<char> content(std::filesystem::file_size(path));
	if (!content.size()) return { };
	in.read(content.data(), content.size());
	if (in.tellg() == content.size()) {
		std::cout << "[textures] " << "File contents loaded: \"" << path.relative_path().string() << "\" (" << content.size() << " bytes)" << std::endl;
		return content;
	}
	std::cout << "[textures] " << rang::fg::red << "Unable to read entire file contents: \"" << path.relative_path().string() << "\"." << rang::fg::reset << std::endl;
	return std::nullopt;
}

namespace textures {

	glm::ivec2 render_target_size { 0, 0 };

	GLuint primary_render_buffer = 0, primary_frame_buffer = 0;
	GLuint deferred_surface_render_target = 0, deferred_position_render_target = 0, deferred_material_render_target = 0;
	GLuint screen_quad_vertex_array = 0, screen_quad_vertex_buffer = 0;
	GLuint cube_primitive_vertex_array = 0, cube_primitive_vertex_buffer = 0;
	GLuint voxel_texture_primary_array = 0;

	std::map<std::string, int> voxel_texture_array_indices;

	auto load_all() -> void {
		if (voxel_texture_primary_array) glDeleteTextures(1, &voxel_texture_primary_array);
		voxel_texture_array_indices.clear();
		int num_files = 0;
		for (auto &i : std::filesystem::directory_iterator("texture")) {
			if (!std::filesystem::is_regular_file(i)) continue;
			num_files++;
		}
		glGenTextures(1, &voxel_texture_primary_array);
		assert(voxel_texture_primary_array);
		glBindTexture(GL_TEXTURE_2D_ARRAY, voxel_texture_primary_array);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 16, 16, num_files, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
		std::cout << "Primary texture array allocated with " << num_files << " indices." << std::endl;
		int index = 0;
		for (auto &i : std::filesystem::directory_iterator("texture")) {
			if (!std::filesystem::is_regular_file(i)) continue;
			auto content = read_file(i.path());
			if (!content) continue;
			int w, h, channels;
			unsigned char *image = stbi_load_from_memory(reinterpret_cast<unsigned char *>(content->data()), content->size(), &w, &h, &channels, STBI_rgb);
			if (!image) {
				std::cout << rang::fg::yellow << "Unable to resolve file contents to an image: \"" << i.path().relative_path().string() << "\"" << rang::fg::reset << std::endl;
				continue;
			}
			if (w == 16 && h == 16) {
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index, w, h, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
				std::cout << "Loaded \"" << i.path().relative_path().string() << "\" into index " << index << "." << std::endl;
				voxel_texture_array_indices[i.path().filename().string()] = index;
				index++;
			} else std::cout << rang::fg::yellow << "Skipped loading image due to invalid size: \"" << i.path().relative_path().string() << "\"" << rang::fg::reset << std::endl;
			stbi_image_free(image);
			num_files--;
			assert(num_files >= 0);
		}
		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	}

	bool generate_render_targets(const glm::ivec2 &size) {

		render_target_size = size;

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

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			std::cout << rang::fg::red << "Failed to create render target for colors." << rang::fg::reset << std::endl;
			return false;
		}

		std::cout << rang::fg::cyan << "All render targets are ready. (" << render_target_size.x << " by " << render_target_size.y << ") " << rang::fg::reset << std::endl;

		GLenum fragment_buffers[] = {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2
		};

		glDrawBuffers(3, fragment_buffers);

		return true;
	}

	void make_screen_quad() {
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
		glBindBuffer(GL_ARRAY_BUFFER, screen_quad_vertex_buffer);
		glBindVertexArray(screen_quad_vertex_array);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void *>(sizeof(float) * 2));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
	}

	void make_cube_primitive() {
		const float points[] = {
			-1, -1, -1,
		};
	}
}

std::map<std::string, GLuint> programs;
std::vector<geometry_point> minimal_point_cache;
std::vector<uint16_t> absolute_point_region(100 * 100 * 100);
GLuint vertex_array, vertex_buffer;
GLint world_transform_location, total_transform_location;

btCompoundShape *test_compound = 0;
btCollisionShape *test_shape = 0;
btDefaultMotionState *test_default_state = 0;
btRigidBody *test_body = 0, *falling_object = 0;

void my_startup() {

	/*
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user_param) {
		std::cout << rang::bg::yellow << rang::fg::black;
		std::cout << message;
		std::cout << rang::bg::reset << rang::fg::reset;
		std::cout << std::endl;
	}, 0);
	*/

	GLint max_draw_buffers;

	glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers);

	std::cout << "Up to " << max_draw_buffers << " draw buffers supported." << std::endl;

	if (auto loaded_programs = shadernaut::launch("glsl/"); loaded_programs) programs = *loaded_programs;
	else {
		goose::sys::kill();
		return;
	}

	textures::make_screen_quad();
	textures::load_all();

	/*
	for (int z = 0; z < 100; z++) {
		for (int y = 0; y < 100; y++) {
			for (int x = 0; x < 100; x++) {
				const int index = (z * 10000) + (y * 100) + x;
				if (x > 20 && x < 80 && y > 20 && y < 80 && z > 20 && z < 80) {
					if (x == 50 && y == 50 && z == 50) absolute_point_region[index] = 1;
					else if (x == 50 && y == 50 && z == 51) absolute_point_region[index] = 1;
					else if (x == 50 && y == 50 && z == 51) absolute_point_region[index] = 1;
					else if (x == 50 && y == 50 && z == 52) absolute_point_region[index] = 1;
					else if (x == 51 && y == 50 && z == 51) absolute_point_region[index] = 1;
					else if (x == 50 && y == 51 && z == 51) absolute_point_region[index] = 1;
					else if (x == 50 && y == 49 && z == 51) absolute_point_region[index] = 1;
					else if (x == 49 && y == 50 && z == 51) absolute_point_region[index] = 1;
					else absolute_point_region[index] = 0;
				} else absolute_point_region[index] = z % 3 && x % 3 && y % 3;
			}
		}
	}
	*/

	glGenVertexArrays(1, &vertex_array);
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBindVertexArray(vertex_array);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_point), 0);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(geometry_point), (void *)(sizeof(GLfloat) * 3));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	world_transform_location = glGetUniformLocation(programs["main"], "world_transform");
	total_transform_location = glGetUniformLocation(programs["main"], "total_transform");

	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 16);

	if (SDL_GL_SetSwapInterval(-1) == -1) {
		SDL_GL_SetSwapInterval(0);
		std::cout << "Immediate buffer swaps enabled." << std::endl;
	} else std::cout << "Late buffer swaps enabled." << std::endl;
}

void my_fixed_update(const double delta) {

	assert(physics::dynamics_world->stepSimulation(delta, 0, 0) == 1);

	/*
	if (test_body) {

		auto &transform = test_body->getWorldTransform();
		auto &origin = transform.getOrigin();
		auto orientation = transform.getRotation();

		glm::fvec3 gl_origin { origin.x(), -origin.z(), origin.y() };

		std::cout << gl_origin.x << " " << gl_origin.y << " " << gl_origin.z << std::endl;
	}
	*/

	static bool first = true;
	if (first) {

		for (int z = 0; z < 100; z++) {
			for (int y = 0; y < 100; y++) {
				for (int x = 0; x < 100; x++) {
					const int index = (z * 10000) + (y * 100) + x;
					if (z == 50 && x >= 49 && x <= 51 && y >= 49 && y <= 51) absolute_point_region[index] = 1;
				}
			}
		}
		minimal_point_cache.clear();
		for (int z = 0; z < 100; z++) {
			for (int y = 0; y < 100; y++) {
				for (int x = 0; x < 100; x++) {
					const int index = (z * 10000) + (y * 100) + x;
					if (!absolute_point_region[index]) continue;
					int face_data = 63;
					const int neighbor_index_left = index - 1;
					const int neighbor_index_right = index + 1;
					const int neighbor_index_below = index - 10000;
					const int neighbor_index_above = index + 10000;
					const int neighbor_index_ahead = index + 100;
					const int neighbor_index_behind = index - 100;
					if (z == 99 || absolute_point_region[neighbor_index_above] != 0) face_data -= 32;
					if (z == 0 || absolute_point_region[neighbor_index_below] != 0) face_data -= 16;
					if (x == 99 || absolute_point_region[neighbor_index_right] != 0) face_data -= 8;
					if (x == 0 || absolute_point_region[neighbor_index_left] != 0) face_data -= 4;
					if (y == 99 || absolute_point_region[neighbor_index_ahead] != 0) face_data -= 2;
					if (y == 0 || absolute_point_region[neighbor_index_behind] != 0) face_data -= 1;
					if (!face_data) continue;
					minimal_point_cache.push_back({
						x - 50.f,
						y - 50.f,
						z - 50.f,
						static_cast<float>(face_data)
					});
				}
			}
		}
		glBindVertexArray(vertex_array);
		glBufferData(GL_ARRAY_BUFFER, sizeof(geometry_point) * minimal_point_cache.size(), minimal_point_cache.data(), GL_STREAM_DRAW);

		std::cout << rang::bg::yellow << rang::fg::black << "Generating physics catastrophe..." << rang::fg::reset << rang::bg::reset << std::endl;

		test_shape = new btBoxShape({ 0.5, 0.5, 0.5 });

		btTransform transform;

		transform.setIdentity();
		test_compound = new btCompoundShape;

		for (const geometry_point &gp : minimal_point_cache) {
			btVector3 corrected_origin = physics::to_bullet(glm::vec3(gp.x, gp.y, gp.z));
			std::cout << "[physics] added @ [" << corrected_origin.x() << " " << corrected_origin.y() << " " << corrected_origin.z() << "]" << std::endl;
			transform.setOrigin(corrected_origin);
			test_compound->addChildShape(transform, new btBoxShape({ 0.5, 0.5, 0.5 }));
		}

		btVector3 local_inertia;

		transform.setIdentity();

		test_compound->calculateLocalInertia(0, local_inertia);
		test_default_state = new btDefaultMotionState(transform);
		test_body = new btRigidBody(0, test_default_state, test_compound, local_inertia);

		physics::dynamics_world->addRigidBody(test_body);

		btTransform aaa;
		
		aaa.setIdentity();
		aaa.setOrigin(physics::to_bullet(glm::vec3(0, 0, 100)));

		btDefaultMotionState *bbb = new btDefaultMotionState(aaa);
		btVector3 ccc;

		test_shape->calculateLocalInertia(1, ccc);

		falling_object = new btRigidBody(1, bbb, test_shape, ccc);

		btVector3 min, max;

		falling_object->getAabb(min, max);

		float mmm = std::max(std::max(max.x() - min.x(), max.y() - min.y()), max.z() - min.z());

		assert(mmm > 0);

		falling_object->setCcdMotionThreshold(0.01);
		falling_object->setCcdSweptSphereRadius(mmm * 0.5);

		physics::dynamics_world->addRigidBody(falling_object);

		first = false;
	}
}

void my_update(const double delta) {

	glm::ivec2 client_area_size;

	SDL_GetWindowSize(goose::sys::sdl_window, &client_area_size.x, &client_area_size.y);

	if (textures::render_target_size != client_area_size) assert(textures::generate_render_targets(client_area_size));

	float render_target_aspect = static_cast<float>(textures::render_target_size.x) / static_cast<float>(textures::render_target_size.y);

	glBindFramebuffer(GL_FRAMEBUFFER, textures::primary_frame_buffer);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, textures::render_target_size.x, textures::render_target_size.y);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	pov::center = { 0, 0, 0 };
	pov::eye = glm::vec4(-3, -3, 4, 1) * glm::rotate(glm::radians(SDL_GetTicks() / 50.f), pov::up);
	pov::view_matrix = glm::lookAt(pov::eye, pov::center, pov::up);

	auto projection = glm::perspective<float>(pov::vertical_fov, render_target_aspect, pov::near, pov::far);
	auto model = glm::identity<glm::mat4>();
	auto total_transform = projection * pov::view_matrix * model;

	glUseProgram(programs["main"]);

	if (world_transform_location != -1) glUniformMatrix4fv(world_transform_location, 1, GL_FALSE, glm::value_ptr(model));
	if (total_transform_location != -1) glUniformMatrix4fv(total_transform_location, 1, GL_FALSE, glm::value_ptr(total_transform));

	glBindVertexArray(vertex_array);
	glDrawArrays(GL_POINTS, 0, minimal_point_cache.size());

	/*
	nvgBeginFrame(goose::sys::nvg_context, view_w, view_h, 1);
	nvgBeginPath(goose::sys::nvg_context);
	nvgCircle(goose::sys::nvg_context, 100, 100, 100);
	nvgFillColor(goose::sys::nvg_context, nvgRGBAf(.8, .8, .8, .5));
	nvgFill(goose::sys::nvg_context);
	nvgEndFrame(goose::sys::nvg_context);
	*/

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glViewport(0, 0, client_area_size.x, client_area_size.y);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glUseProgram(programs["screen"]);

	static float sharpening_power = 0;
	static float gamma_power = 1.5;
	static bool show_render_buffers = false;

	GLint pixel_w_location = glGetUniformLocation(programs["screen"], "pixel_w");
	GLint pixel_h_location = glGetUniformLocation(programs["screen"], "pixel_h");
	GLint sharpening_power_location = glGetUniformLocation(programs["screen"], "sharpening_power");
	GLint gamma_power_location = glGetUniformLocation(programs["screen"], "gamma_power");

	if (pixel_w_location != -1) glUniform1f(pixel_w_location, 1.f / static_cast<float>(textures::render_target_size.x));
	if (pixel_h_location != -1) glUniform1f(pixel_h_location, 1.f / static_cast<float>(textures::render_target_size.y));
	if (sharpening_power_location != -1) glUniform1f(sharpening_power_location, sharpening_power);
	if (gamma_power_location != -1) glUniform1f(gamma_power_location, gamma_power);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures::deferred_surface_render_target);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures::deferred_position_render_target);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, textures::deferred_material_render_target);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures::voxel_texture_primary_array);
	glBindVertexArray(textures::screen_quad_vertex_array);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(goose::sys::sdl_window);
	ImGui::NewFrame();
	ImGui::Text(fmt::format("Frame Delta: {}", delta));
	ImGui::Text(fmt::format("Frame Time: {}", static_cast<int>(delta * 1000.0)));
	ImGui::Text(fmt::format("Frames per Second: {}", static_cast<int>(1.0 / delta)));
	ImGui::SliderFloat("Sharpening", &sharpening_power, 0, 1);
	ImGui::SliderFloat("Gamma", &gamma_power, 0.5, 3);
	ImGui::Checkbox("Show Render Buffers", &show_render_buffers);

	if (show_render_buffers) {

		float aspect_ratio = static_cast<float>(textures::render_target_size.x) / static_cast<float>(textures::render_target_size.y);

		ImGui::GetBackgroundDrawList()->AddImageRounded(
			reinterpret_cast<void *>(textures::deferred_surface_render_target),
			{ 10, 10 }, { 210 * aspect_ratio, 210 },
			{ 0, 0 }, { 1, 1 },
			IM_COL32(255, 255, 255, 255), 9
		);

		ImGui::GetBackgroundDrawList()->AddImageRounded(
			reinterpret_cast<void *>(textures::deferred_position_render_target),
			{ 10, 220 }, { 210 * aspect_ratio, 420 },
			{ 0, 0 }, { 1, 1 },
			IM_COL32(255, 255, 255, 255), 9
		);

		ImGui::GetBackgroundDrawList()->AddImageRounded(
			reinterpret_cast<void *>(textures::deferred_material_render_target),
			{ 10, 430 }, { 210 * aspect_ratio, 630 },
			{ 0, 0 }, { 1, 1 },
			IM_COL32(255, 255, 255, 255), 9
		);
	}

	if (falling_object) {

		auto &transform = falling_object->getWorldTransform();
		auto &origin = transform.getOrigin();
		auto orientation = transform.getRotation();

		glm::fvec3 gl_origin { origin.x(), origin.y(), origin.z() };

		btVector3 lll = falling_object->getLinearVelocity();

		std::string ttt = fmt::format("[physics] @ {:.{}f} {:.{}f} {:.{}f}, {:.{}f} {:.{}f} {:.{}f}", gl_origin.x, 2, gl_origin.y, 2, gl_origin.z, 2, lll.x(), 2, lll.y(), 2, lll.z(), 2);

		ImGui::Text(ttt.c_str());
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(goose::sys::sdl_window);
}

int main() {
	physics::initialize();
	goose::sys::on_startup = my_startup;
	goose::sys::on_update = my_update;
	goose::sys::on_fixed_step = my_fixed_update;
	if (!goose::sys::boot()) return 1;
	while (goose::sys::update());
	goose::sys::kill();
	physics::cleanup();
	return 0;
}