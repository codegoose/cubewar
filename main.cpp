#define WIN32_LEAN_AND_MEAN
#include <rang.hpp>
#define SDL_MAIN_HANDLED
#include <sdl.h>
#define GLEW_STATIC
#include <gl/glew.h>
#include <nanovg.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <enet/enet.h>
#include <btBulletDynamicsCommon.h>
#include <json.hpp>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <map>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#undef min
#undef max
#undef near
#undef far

/*

	Tick Frequency Settings

*/

const uint64_t fixed_steps_per_second = 100;
uint64_t performance_frequency = 0;
uint64_t last_performance_counter = 0;
double variable_time_delta = 0;
const double fixed_step_time_delta = 1. / fixed_steps_per_second;
uint64_t num_performance_counters_per_fixed_step = 0;
uint64_t fixed_step_counter_remainder = 0;
bool is_performance_optimal = true;

/*

	Initialization State

*/

bool sdl_initialized = false;
SDL_Window *sdl_window = 0;
SDL_GLContext gl_context;
NVGcontext *nvg_context = 0;
ImGuiContext *imgui_context = 0;
bool imgui_sdl_impl_initialized = false;
bool imgui_opengl3_impl_initialized = false;
bool enet_initialized = false;

/*

	Linked GPU programs.

*/

std::map<std::string, GLuint> gpu_programs;

/*

	Physics State

*/

btDefaultCollisionConfiguration *collision_configuration = 0;
btCollisionDispatcher *collision_dispatcher = 0;
btBroadphaseInterface *broadphase_interface = 0;
btSequentialImpulseConstraintSolver *constraint_solver = 0;
btDiscreteDynamicsWorld *dynamics_world = 0;

/*

	Utility

*/

std::optional<std::vector<char>> read_file(const std::filesystem::path &path) {
	std::ifstream in(path.string(), std::ios::binary);
	if (!in.is_open()) {
		std::cout << rang::fg::yellow << "Failed to open file for reading: " << path.string() << rang::fg::reset << std::endl;
		return std::nullopt;
	}
	std::vector<char> content(std::filesystem::file_size(path));
	if (!content.size()) return { };
	in.read(content.data(), content.size());
	if (in.tellg() == content.size()) {
		std::cout << "File contents loaded: \"" << path.relative_path().string() << "\" (" << content.size() << " bytes)" << std::endl;
		return content;
	}
	std::cout << rang::fg::red << "Unable to read entire file contents: \"" << path.relative_path().string() << "\"." << rang::fg::reset << std::endl;
	return std::nullopt;
}

std::optional<std::map<std::string, std::vector<std::string>>> map_file_names_and_extensions(const std::filesystem::path &path) {
	if (!std::filesystem::exists(path)) return std::nullopt;
	if (!std::filesystem::is_directory(path)) return std::nullopt;
	std::map<std::string, std::vector<std::string>> map;
	for (auto &i : std::filesystem::directory_iterator(path)) {
		if (!i.path().has_extension()) continue;
		map[i.path().stem().string()].push_back(i.path().extension().string());
	}
	return map;
}

/*

	Configurations

*/

const char *cfg_file = "settings.json";
nlohmann::json cfg;

void load_cfg() {
	if (!std::filesystem::exists(cfg_file)) {
		std::cout << rang::fg::yellow << "No configurations loaded because the file doesn't exist." << rang::fg::reset << std::endl;
		return;
	}
	std::ifstream in(cfg_file);
	if (!in.is_open()) {
		std::cout << rang::fg::yellow << "No configurations loaded because the file couldn't be opened." << rang::fg::reset << std::endl;
		return;
	}
	std::stringstream content;
	content << in.rdbuf();
	cfg = nlohmann::json::parse(content.str());
}

void flush_cfg() {
	if (cfg.empty()) return;
	std::ofstream ou(cfg_file);
	if (!ou.is_open()) {
		std::cout << rang::fg::yellow << "No configurations saved because the file couldn't be opened." << rang::fg::reset << std::endl;
		return;
	}
	auto content = cfg.dump(1, ' ', true);
	ou.write(content.data(), content.length());
	std::cout << "Configurations saved." << std::endl;
}

/*

	Phyics

*/

void cleanup_physics() {
	if (dynamics_world) delete dynamics_world;
	if (constraint_solver) delete constraint_solver;
	if (broadphase_interface) delete broadphase_interface;
	if (collision_dispatcher) delete collision_dispatcher;
	if (collision_configuration) delete collision_configuration;
	dynamics_world = 0;
	constraint_solver = 0;
	broadphase_interface = 0;
	collision_dispatcher = 0;
	collision_configuration = 0;
}

void initialize_physics() {
	collision_configuration = new btDefaultCollisionConfiguration;
	collision_dispatcher = new btCollisionDispatcher(collision_configuration);
	broadphase_interface = new btDbvtBroadphase;
	constraint_solver = new btSequentialImpulseConstraintSolver;
	dynamics_world = new btDiscreteDynamicsWorld(collision_dispatcher, broadphase_interface, constraint_solver, collision_configuration);
	dynamics_world->setGravity({ 0, -10, 0 });
}

glm::vec3 from_physics_world(const btVector3 &in) {
	return { in.x(), -in.z(), in.y() };
}

btVector3 to_physics_world(const glm::vec3 &in) {
	return { in.x, in.z, -in.y };
}

glm::quat from_physics_world(const btQuaternion &in) {
	glm::quat q;
	q.w = in.w();
	q.x = in.x();
	q.y = -in.z();
	q.z = in.y();
	return q;
}

btQuaternion to_physics_world(const glm::quat &in) {
	btQuaternion q;
	q.setW(in.x);
	q.setX(in.z);
	q.setY(-in.y);
	q.setZ(in.w);
	return q;
}

/*

	Shutdown

*/

void kill() {
	if (sdl_window) SDL_HideWindow(sdl_window);
	if (enet_initialized) {
		enet_deinitialize();
		enet_initialized = false;
		std::cout << "Shutdown ENet." << std::endl;
	}
	if (imgui_opengl3_impl_initialized) {
		ImGui_ImplOpenGL3_Shutdown();
		std::cout << "Cleaned up ImGui OpenGL implemenation." << std::endl;
	}
	if (imgui_sdl_impl_initialized) {
		ImGui_ImplSDL2_Shutdown();
		std::cout << "Cleaned up ImGui SDL2 implementation." << std::endl;
	}
	if (imgui_context) {
		ImGui::DestroyContext(imgui_context);
		std::cout << "Destroyed ImGui context." << std::endl;
	}
	if (nvg_context) {
		nvgDeleteGL3(nvg_context);
		std::cout << "Destroyed NanoVG context." << std::endl;
	}
	if (gl_context) {
		SDL_GL_DeleteContext(gl_context);
		std::cout << "Destroyed SDL2 OpenGL context." << std::endl;
	}
	if (sdl_window) {
		SDL_DestroyWindow(sdl_window);
		std::cout << "Destroyed SDL2 window." << std::endl;
	}
	imgui_opengl3_impl_initialized = false;
	imgui_sdl_impl_initialized = false;
	imgui_context = 0;
	nvg_context = 0;
	gl_context = 0;
	sdl_window = 0;
	if (sdl_initialized) {
		SDL_Quit();
		sdl_initialized = false;
	}
	flush_cfg();
}

/*

	Shaders

*/

void print_program_info_log(GLuint id) {
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

GLuint make_program_from_shaders(const std::vector<GLuint> &shaders) {
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

void print_shader_info_log(GLuint id) {
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

GLuint make_shader_from_file(const std::filesystem::path &path) {
	auto content = read_file(path);
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

std::optional<std::map<std::string, GLuint>> make_programs_from_directory(const std::filesystem::path &path) {
	auto map = map_file_names_and_extensions(path);
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

/*

	Rendering

*/

glm::ivec2 render_target_size { 0, 0 };
GLuint primary_render_buffer = 0, primary_frame_buffer = 0;
GLuint deferred_surface_render_target = 0, deferred_position_render_target = 0, deferred_material_render_target = 0;
GLuint screen_quad_vertex_array = 0, screen_quad_vertex_buffer = 0;

GLuint voxel_vertex_array = 0, voxel_vertex_buffer = 0;
std::map<std::string, int> voxel_texture_array_indices;
struct voxel_geometry_point { GLfloat x, y, z, meta; };
std::vector<voxel_geometry_point> voxel_point_cache;
std::vector<uint16_t> voxel_point_region(100 * 100 * 100);

GLuint texture_voxel_array = 0, texture_256_array = 0, texture_512_array = 0;

struct mesh {

	struct vertex {

		glm::vec3 position, normal;
		glm::vec2 uv;
	};

	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint num_vertices;
	GLuint texture;
};

std::map<std::string, mesh> objects;

void load_voxel_textures() {
	if (texture_voxel_array) glDeleteTextures(1, &texture_voxel_array);
	voxel_texture_array_indices.clear();
	int num_files = 0;
	for (auto &i : std::filesystem::directory_iterator("texture\\face")) {
		if (!std::filesystem::is_regular_file(i)) continue;
		num_files++;
	}
	glGenTextures(1, &texture_voxel_array);
	assert(texture_voxel_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_voxel_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 16, 16, num_files, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	std::cout << "Primary texture array allocated with " << num_files << " indices." << std::endl;
	int index = 0;
	for (auto &i : std::filesystem::directory_iterator("texture\\face")) {
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
}

void unload_objects() {
	for (auto &object : objects) {
		if (object.second.texture) glDeleteTextures(1, &object.second.texture);
	}
	objects.clear();
}

/*
void load_object_palette_textures() {
	if (object_palette_texture_array) glDeleteTextures(1, &object_palette_texture_array);
	// voxel_texture_array_indices.clear();
	int num_files = 0;
	for (auto &i : std::filesystem::directory_iterator("texture\\palette")) {
		if (!std::filesystem::is_regular_file(i)) continue;
		num_files++;
	}
	glGenTextures(1, &object_palette_texture_array);
	assert(object_palette_texture_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, object_palette_texture_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 256, 1, num_files, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	std::cout << "Object color palette texture array allocated with " << num_files << " indices." << std::endl;
	int index = 0;
	for (auto &i : std::filesystem::directory_iterator("texture\\palette")) {
		if (!std::filesystem::is_regular_file(i)) continue;
		auto content = read_file(i.path());
		if (!content) continue;
		int w, h, channels;
		unsigned char *image = stbi_load_from_memory(reinterpret_cast<unsigned char *>(content->data()), content->size(), &w, &h, &channels, STBI_rgb);
		if (!image) {
			std::cout << rang::fg::yellow << "Unable to resolve file contents to an image: \"" << i.path().relative_path().string() << "\"" << rang::fg::reset << std::endl;
			continue;
		}
		if (w == 256 && h == 1) {
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index, w, h, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
			std::cout << "Loaded object palette texture \"" << i.path().relative_path().string() << "\" into index " << index << "." << std::endl;
			// voxel_texture_array_indices[i.path().filename().string()] = index;
			index++;
		} else std::cout << rang::fg::yellow << "Skipped loading image due to invalid size: \"" << i.path().relative_path().string() << "\"" << rang::fg::reset << std::endl;
		stbi_image_free(image);
		num_files--;
		assert(num_files >= 0);
	}
}
*/

void load_object_textures() {
	std::cout << rang::fg::cyan << "Beginning x256/x512 object texture processing now." << rang::fg::reset << std::endl;
	auto items = map_file_names_and_extensions("object");
	if (!items) return;
	std::map<std::string, std::pair<std::vector<char>, glm::ivec2>> textures_256, textures_512;
	for (auto &item : *items) {
		auto texture_path = fmt::format("object\\{}.png", item.first);
		auto texture_file_contents = read_file(texture_path);
		if (!texture_file_contents) continue;
		int w, h, channels;
		unsigned char *image = stbi_load_from_memory(
			reinterpret_cast<unsigned char *>(texture_file_contents->data()),
			texture_file_contents->size(),
			&w, &h, &channels, STBI_rgb
		);
		if (!image) {
			std::cout << rang::bg::yellow << rang::fg::black;
			std::cout << "Unable to recognize file \"" << texture_path << "\" as an image.";
			std::cout << rang::bg::reset << rang::fg::reset << std::endl;
			continue;
		}
		if (w == 256 && h == 256) {
			textures_256[item.first].first.resize(w * h * 3);
			textures_256[item.first].second = { w, h };
			memcpy(textures_256[item.first].first.data(), image, w * h * 3);
		} else if (w == 512 && h == 512) {
			textures_512[item.first].first.resize(w * h * 3);
			textures_512[item.first].second = { w, h };
			memcpy(textures_512[item.first].first.data(), image, w * h * 3);
		} else {
			std::cout << rang::bg::yellow << rang::fg::black;
			std::cout << "Texture within \"" << texture_path << "\" is an unsupported size. (" << w << " by " << h << ")";
			std::cout << rang::bg::reset << rang::fg::reset << std::endl;
		}
		stbi_image_free(image);
	}
	std::cout << "Processed image data for " << textures_256.size() << " x256 textures." << std::endl;
	std::cout << "Processed image data for " << textures_512.size() << " x512 textures." << std::endl;
	std::cout << rang::fg::cyan << "Beginning x256/x512 object texture GPU upload now." << rang::fg::reset << std::endl;
	if (texture_256_array) glDeleteTextures(1, &texture_256_array);
	if (texture_512_array) glDeleteTextures(1, &texture_512_array);
	glGenTextures(1, &texture_256_array);
	assert(texture_256_array);
	glGenTextures(1, &texture_512_array);
	assert(texture_512_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_256_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 256, 256, textures_256.size(), 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLint z_offset = 0;
	for (auto &image : textures_256) {
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0,
			z_offset, image.second.second.x, image.second.second.y, 1,
			GL_RGB, GL_UNSIGNED_BYTE, image.second.first.data());
		std::cout << "Loaded \"" << image.first << "\"" << " x256 texture array at location " << z_offset << "." << std::endl;
		z_offset++;
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_512_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 512, 512, textures_512.size(), 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	z_offset = 0;
	for (auto &image : textures_512) {
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0,
			z_offset, image.second.second.x, image.second.second.y, 1,
			GL_RGB, GL_UNSIGNED_BYTE, image.second.first.data());
		std::cout << "Loaded \"" << image.first << "\"" << " x512 texture array at location " << z_offset << "." << std::endl;
		z_offset++;
	}
	std::cout << "Finished object texture loading." << std::endl;
}

void load_objects() {
	unload_objects();
	auto items = map_file_names_and_extensions("object");
	if (!items) return;
	for (auto &e : *items) {
		std::cout << e.first << " -> ";
		for (auto &ee : e.second) std::cout << ee << " ";
		std::cout << std::endl;
		auto file_contents = read_file(fmt::format("object\\{}{}", e.first, ".obj"));
		if (!file_contents) continue;
		Assimp::Importer importer;
		auto scene = importer.ReadFileFromMemory(
			file_contents->data(),
			file_contents->size(),
			aiProcess_FlipUVs | aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes);
		if (!scene) continue;
		assert(scene->mNumMeshes == 1);
		mesh new_mesh;
		std::vector<mesh::vertex> vertices;
		for (unsigned int face_index = 0; face_index < scene->mMeshes[0]->mNumFaces; face_index++) {
			for (unsigned int i = 0; i < scene->mMeshes[0]->mFaces[face_index].mNumIndices; i++) {
				const auto position = scene->mMeshes[0]->mVertices[scene->mMeshes[0]->mFaces[face_index].mIndices[i]];
				const auto normal = scene->mMeshes[0]->mNormals[scene->mMeshes[0]->mFaces[face_index].mIndices[i]];
				const auto uv = scene->mMeshes[0]->mTextureCoords[0][scene->mMeshes[0]->mFaces[face_index].mIndices[i]];
				vertices.push_back({
					{ position.x, position.y, position.z },
					{ normal.x, normal.y, normal.z },
					{ uv.x, uv.y }
				});
			}
		}
		glGenVertexArrays(1, &new_mesh.vertex_array);
		assert(new_mesh.vertex_array);
		glGenBuffers(1, &new_mesh.vertex_buffer);
		assert(new_mesh.vertex_buffer);
		glBindVertexArray(new_mesh.vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, new_mesh.vertex_buffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(mesh::vertex), 0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(mesh::vertex), reinterpret_cast<void *>(sizeof(float) * 3));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(mesh::vertex), reinterpret_cast<void *>(sizeof(float) * 6));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(mesh::vertex), vertices.data(), GL_STATIC_DRAW);
		new_mesh.texture = 0;
		new_mesh.num_vertices = vertices.size();
		objects[e.first] = new_mesh;
		std::cout << "Loaded object: " << e.first << std::endl;
	}
}

void render_voxel_world() {
	static bool first = true;
	if (first) {
		std::cout << rang::bg::yellow << rang::fg::black;
		std::cout << "Uploading voxels to GPU...";
		std::cout << rang::bg::reset << rang::fg::reset << std::endl;
		for (int z = 0; z < 100; z++) {
			for (int y = 0; y < 100; y++) {
				for (int x = 0; x < 100; x++) {
					const int index = (z * 10000) + (y * 100) + x;
					if (z == 50) voxel_point_region[index] = 1;
				}
			}
		}
		voxel_point_cache.clear();
		for (int z = 0; z < 100; z++) {
			for (int y = 0; y < 100; y++) {
				for (int x = 0; x < 100; x++) {
					const int index = (z * 10000) + (y * 100) + x;
					if (!voxel_point_region[index]) continue;
					int face_data = 63;
					const int neighbor_index_left = index - 1;
					const int neighbor_index_right = index + 1;
					const int neighbor_index_below = index - 10000;
					const int neighbor_index_above = index + 10000;
					const int neighbor_index_ahead = index + 100;
					const int neighbor_index_behind = index - 100;
					if (z == 99 || voxel_point_region[neighbor_index_above] != 0) face_data -= 32;
					if (z == 0 || voxel_point_region[neighbor_index_below] != 0) face_data -= 16;
					if (x == 99 || voxel_point_region[neighbor_index_right] != 0) face_data -= 8;
					if (x == 0 || voxel_point_region[neighbor_index_left] != 0) face_data -= 4;
					if (y == 99 || voxel_point_region[neighbor_index_ahead] != 0) face_data -= 2;
					if (y == 0 || voxel_point_region[neighbor_index_behind] != 0) face_data -= 1;
					if (!face_data) continue;
					voxel_point_cache.push_back({
						x - 50.f,
						y - 50.f,
						z - 50.f,
						static_cast<float>(face_data)
					});
				}
			}
		}
		glBindVertexArray(voxel_vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, voxel_vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(voxel_geometry_point) * voxel_point_cache.size(), voxel_point_cache.data(), GL_STREAM_DRAW);
		first = false;
	}
	glBindVertexArray(voxel_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, voxel_vertex_buffer);
	glDrawArrays(GL_POINTS, 0, voxel_point_cache.size());
}

void prepare_voxel_array() {
	glGenVertexArrays(1, &voxel_vertex_array);
	glGenBuffers(1, &voxel_vertex_buffer);
	glBindVertexArray(voxel_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, voxel_vertex_buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(voxel_geometry_point), 0);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(voxel_geometry_point), (void *)(sizeof(GLfloat) * 3));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
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
	glBindVertexArray(screen_quad_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, screen_quad_vertex_buffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void *>(sizeof(float) * 2));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
}

void generate_render_targets() {
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

/*

	Loop
	Fixed Update

*/

btCompoundShape *test_compound = 0;
btCollisionShape *test_shape = 0;
btDefaultMotionState *test_default_state = 0;
btRigidBody *test_body = 0, *falling_object = 0;

namespace pov {

	glm::vec3 eye { 0, 0, 0 } , center { 0, 0, 0 }, look { 0, 0, 0 };
	glm::quat orientation { glm::identity<glm::quat>() };
	float vertical_fov = glm::radians(85.0f);
	float near = 0.01, far = 100.01;
	glm::mat4 view_matrix { glm::identity<glm::mat4>() };
	const glm::vec3 up { 0, 0, 1 };
}

void on_fixed_step(double delta) {
	assert(dynamics_world->stepSimulation(delta, 0, 0) == 1);
}

void on_update(double delta) {
	static bool show_wireframe = false;
	glm::ivec2 client_area_size;
	SDL_GetWindowSize(sdl_window, &client_area_size.x, &client_area_size.y);
	if (render_target_size != client_area_size) {
		render_target_size = client_area_size;
		generate_render_targets();
	}
	float render_target_aspect = static_cast<float>(render_target_size.x) / static_cast<float>(render_target_size.y);
	glBindFramebuffer(GL_FRAMEBUFFER, primary_frame_buffer);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, render_target_size.x, render_target_size.y);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, show_wireframe ? GL_LINE : GL_FILL);
	glLineWidth(2);
	pov::center = { 0, 0, 0.5 };
	pov::eye = glm::vec4(-3, -3, 2, 1) * glm::rotate(glm::radians(SDL_GetTicks() / 150.f), pov::up);
	pov::view_matrix = glm::lookAt(pov::eye, pov::center, pov::up);
	{
		auto projection = glm::perspective<float>(pov::vertical_fov, render_target_aspect, pov::near, pov::far);
		auto model = glm::identity<glm::mat4>();
		auto total_transform = projection * pov::view_matrix * model;
		glUseProgram(gpu_programs["main"]);
		GLint world_transform_location = glGetUniformLocation(gpu_programs["main"], "world_transform");
		GLint total_transform_location = glGetUniformLocation(gpu_programs["main"], "total_transform");
		if (world_transform_location != -1) glUniformMatrix4fv(world_transform_location, 1, GL_FALSE, glm::value_ptr(model));
		if (total_transform_location != -1) glUniformMatrix4fv(total_transform_location, 1, GL_FALSE, glm::value_ptr(total_transform));
		render_voxel_world();
	}
	{
		auto projection = glm::perspective<float>(pov::vertical_fov, render_target_aspect, pov::near, pov::far);
		auto model =
			glm::translate(glm::identity<glm::mat4>(), glm::vec3(0, 0, 0.5f))
			* glm::rotate(glm::radians(90.0f), glm::vec3(1, 0, 0))
			* glm::scale(glm::vec3(1.0f));
		auto total_transform = projection * pov::view_matrix * model;
		glUseProgram(gpu_programs["mesh"]);
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
		glUniform2f(glGetUniformLocation(gpu_programs["mesh"], "material_identifier"), 0, 1000);
		glBindVertexArray(objects["fireowl-breaker"].vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, objects["fireowl-breaker"].vertex_buffer);
		glDrawArrays(GL_TRIANGLES, 0, objects["fireowl-breaker"].num_vertices);
	}
	{
		auto projection = glm::perspective<float>(pov::vertical_fov, render_target_aspect, pov::near, pov::far);
		auto model =
			glm::translate(glm::identity<glm::mat4>(), glm::vec3(-1.5, -1.5, 0.5f))
			* glm::rotate(glm::radians(90.0f), glm::vec3(1, 0, 0))
			* glm::scale(glm::vec3(1.0f));
		auto total_transform = projection * pov::view_matrix * model;
		glUseProgram(gpu_programs["mesh"]);
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
		glUniform2f(glGetUniformLocation(gpu_programs["mesh"], "material_identifier"), 0, 1001);
		glBindVertexArray(objects["sniper"].vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, objects["sniper"].vertex_buffer);
		glDrawArrays(GL_TRIANGLES, 0, objects["sniper"].num_vertices);
	}
	{
		auto projection = glm::perspective<float>(pov::vertical_fov, render_target_aspect, pov::near, pov::far);
		auto model =
			glm::translate(glm::identity<glm::mat4>(), glm::vec3(0, 2, 0.5f))
			* glm::rotate(glm::radians(90.0f), glm::vec3(1, 0, 0))
			* glm::scale(glm::vec3(1.0f));
		auto total_transform = projection * pov::view_matrix * model;
		glUseProgram(gpu_programs["mesh"]);
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
		glUniform2f(glGetUniformLocation(gpu_programs["mesh"], "material_identifier"), 0, 1002);
		glBindVertexArray(objects["hmg"].vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, objects["hmg"].vertex_buffer);
		glDrawArrays(GL_TRIANGLES, 0, objects["hmg"].num_vertices);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glViewport(0, 0, client_area_size.x, client_area_size.y);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glUseProgram(gpu_programs["screen"]);
	static float saturation_power = 1;
	static float gamma_power = 1.5;
	static bool show_render_buffers = false;
	GLint pixel_w_location = glGetUniformLocation(gpu_programs["screen"], "pixel_w");
	GLint pixel_h_location = glGetUniformLocation(gpu_programs["screen"], "pixel_h");
	GLint saturation_power_location = glGetUniformLocation(gpu_programs["screen"], "saturation_power");
	GLint gamma_power_location = glGetUniformLocation(gpu_programs["screen"], "gamma_power");
	GLint near_plane_location = glGetUniformLocation(gpu_programs["screen"], "near_plane");
	GLint far_plane_location = glGetUniformLocation(gpu_programs["screen"], "far_plane");
	if (pixel_w_location != -1) glUniform1f(pixel_w_location, 1.f / static_cast<float>(render_target_size.x));
	if (pixel_h_location != -1) glUniform1f(pixel_h_location, 1.f / static_cast<float>(render_target_size.y));
	if (saturation_power_location != -1) glUniform1f(saturation_power_location, saturation_power);
	if (gamma_power_location != -1) glUniform1f(gamma_power_location, gamma_power);
	if (near_plane_location != -1) glUniform1f(near_plane_location, pov::near);
	if (far_plane_location != -1) glUniform1f(far_plane_location, pov::far);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, deferred_surface_render_target);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, deferred_position_render_target);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, deferred_material_render_target);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_voxel_array);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_256_array);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_512_array);
	glBindVertexArray(screen_quad_vertex_array);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(sdl_window);
	ImGui::NewFrame();
	ImGui::Text(fmt::format("Frame Delta: {}", delta).c_str());
	ImGui::Text(fmt::format("Frame Time: {}", static_cast<int>(delta * 1000.0)).c_str());
	ImGui::Text(fmt::format("Frames per Second: {}", static_cast<int>(1.0 / delta)).c_str());
	ImGui::Separator();
	ImGui::SliderFloat("Gamma", &gamma_power, 0.5, 3);
	ImGui::Checkbox("Show Wireframe", &show_wireframe);
	ImGui::Checkbox("Show Render Buffers", &show_render_buffers);
	if (show_render_buffers) {
		float aspect_ratio = static_cast<float>(render_target_size.x) / static_cast<float>(render_target_size.y);
		ImGui::GetBackgroundDrawList()->AddImageRounded(
			reinterpret_cast<void *>(deferred_surface_render_target),
			{ 10, 10 }, { 210 * aspect_ratio, 210 },
			{ 0, 0 }, { 1, 1 },
			IM_COL32(255, 255, 255, 255), 9
		);
		ImGui::GetBackgroundDrawList()->AddImageRounded(
			reinterpret_cast<void *>(deferred_position_render_target),
			{ 10, 220 }, { 210 * aspect_ratio, 420 },
			{ 0, 0 }, { 1, 1 },
			IM_COL32(255, 255, 255, 255), 9
		);
		ImGui::GetBackgroundDrawList()->AddImageRounded(
			reinterpret_cast<void *>(deferred_material_render_target),
			{ 10, 430 }, { 210 * aspect_ratio, 630 },
			{ 0, 0 }, { 1, 1 },
			IM_COL32(255, 255, 255, 255), 9
		);
	}
	ImGui::GetBackgroundDrawList()->AddText({ 10, 10 }, IM_COL32(255, 255, 255, 255), "Test");
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(sdl_window);
	//
}

bool tick() {
	SDL_Event os_event;
	bool quit_signal = false;
	while (SDL_PollEvent(&os_event)) {
		ImGui_ImplSDL2_ProcessEvent(&os_event);
		if (os_event.type == SDL_QUIT) quit_signal = true;
	}
	if (!performance_frequency) {
		performance_frequency = SDL_GetPerformanceFrequency();
		num_performance_counters_per_fixed_step = performance_frequency / fixed_steps_per_second;
		last_performance_counter = SDL_GetPerformanceCounter();
		return true;
	}
	const uint64_t performance_counter = SDL_GetPerformanceCounter();
	const uint64_t elapsed_performance_counter = performance_counter - last_performance_counter;
	fixed_step_counter_remainder += elapsed_performance_counter;
	uint32_t num_fixed_steps_this_i = 0;
	while (fixed_step_counter_remainder >= num_performance_counters_per_fixed_step) {
		on_fixed_step(fixed_step_time_delta);
		fixed_step_counter_remainder -= num_performance_counters_per_fixed_step;
		num_fixed_steps_this_i++;
	}
	is_performance_optimal = num_fixed_steps_this_i <= 1;
	last_performance_counter = performance_counter;
	on_update(static_cast<double>(elapsed_performance_counter) / performance_frequency);
	return !quit_signal;
}

/*

	Entry Point

*/

int main() {
	load_cfg();
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		std::cout << rang::fg::red << "Failed to initialize SDL2." << rang::fg::reset << std::endl;
		return 1;
	} else std::cout << "SDL2 subsystems are ready." << std::endl;
	sdl_initialized = true;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	sdl_window = SDL_CreateWindow("CubeWar", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 960, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
	if (!sdl_window) {
		kill();
		return 2;
	}
	gl_context = SDL_GL_CreateContext(sdl_window);
	if (!gl_context) {
		std::cout << rang::fg::red << "Failed to create OpenGL context." << rang::fg::reset << std::endl;
		kill();
		return 3;
	}
	else std::cout << "OpenGL context created: " << glGetString(GL_VERSION) << " (" << glGetString(GL_RENDERER) << ")" << std::endl;
	static bool glew_init_already = false;
	if (!glew_init_already) {
		glewExperimental = true;
		if (glewInit()) {
			std::cout << "Failed to wrangle OpenGL extensions." << std::endl;
			kill();
			return 4;
		} else std::cout << "OpenGL extensions wrangled." << std::endl;
		glew_init_already = true;
	}
	nvg_context = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
	if (!nvg_context) {
		std::cout << rang::fg::red << "Failed to create NanoVG context." << rang::fg::reset << std::endl;
		kill();
		return 5;
	} else std::cout << "NanoVG context is ready." << std::endl;
	imgui_context = ImGui::CreateContext();
	if (!imgui_context) {
		std::cout << rang::fg::red << "Failed to create ImGui context." << rang::fg::reset << std::endl;
		kill();
		return 6;
	}
	std::cout << "ImGui context is ready." << std::endl;
	if (!ImGui_ImplSDL2_InitForOpenGL(sdl_window, gl_context)) {
		imgui_sdl_impl_initialized = false;
		std::cout << rang::fg::red << "Failed to prepare ImGui context for SDL2 input." << rang::fg::reset << std::endl;
		kill();
		return 7;
	}
	imgui_sdl_impl_initialized = true;
	if (!ImGui_ImplOpenGL3_Init("#version 130")) {
		imgui_opengl3_impl_initialized = false;
		std::cout << rang::fg::red << "Failed to prepare ImGui context for OpenGL rendering." << rang::fg::reset << std::endl;
		kill();
		return 8;
	}
	imgui_opengl3_impl_initialized = true;
	ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 16);
	std::cout << "Integrated ImGui with SDL2 and OpenGL." << std::endl;
	if (enet_initialize() < 0) {
		enet_initialized = false;
		std::cout << rang::fg::red << "Failed to startup ENet." << rang::fg::reset << std::endl;
		kill();
		return 9;
	}
	std::cout << "ENet is ready." << std::endl;
	enet_initialized = true;
	if (auto programs = make_programs_from_directory("glsl/"); programs.has_value()) gpu_programs = *programs;
	else {
		std::cout << rang::fg::red << "Failed to create GPU programs." << rang::fg::reset << std::endl;
		kill();
		return 10;
	}
	initialize_physics();
	make_screen_quad();
	load_voxel_textures();
	prepare_voxel_array();
	load_objects();
	load_object_textures();
	SDL_ShowWindow(sdl_window);
	while (tick());
	SDL_HideWindow(sdl_window);
	cleanup_physics();
	kill();
	return 0;
}