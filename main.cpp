#define WIN32_LEAN_AND_MEAN
#include <rang.hpp>
#include <btBulletDynamicsCommon.h>
#include <string>
#include <memory>
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

#include "pov.h"
#include "misc.h"

/*

	Player State

*/

btMotionState *player_motion_state = 0;
btCollisionShape *player_collision_shape = 0;
btRigidBody *player_rigid_body = 0;
bool player_binary_input_left = false;
bool player_binary_input_right = false;
bool player_binary_input_forward = false;
bool player_binary_input_backward = false;
glm::vec3 player_location_interpolation_pair[2];
glm::vec3 player_interpolated_location;

void shutdown_player() {
	if (player_rigid_body) {
		dynamics_world->removeRigidBody(player_rigid_body);
		delete player_rigid_body;
		player_rigid_body = 0;
	}
	if (player_collision_shape) {
		delete player_collision_shape;
		player_collision_shape = 0;
	}
	if (player_motion_state) {
		delete player_motion_state;
		player_motion_state = 0;
	}
}

void initialize_player() {
	shutdown_player();
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(to_physics_world(glm::vec3(0, 0, 150)));
	player_motion_state = new btDefaultMotionState(transform);
	player_collision_shape = new btCapsuleShape(0.4f, 1.0f);
	btVector3 local_inertia;
	player_collision_shape->calculateLocalInertia(1, local_inertia);
	player_rigid_body = new btRigidBody(1, player_motion_state, player_collision_shape, local_inertia);
	player_rigid_body->setAngularFactor(0);
	player_rigid_body->setActivationState(DISABLE_DEACTIVATION);
	dynamics_world->addRigidBody(player_rigid_body, 2, 1);
}

GLuint voxel_vertex_array = 0, voxel_vertex_buffer = 0;
std::map<std::string, int> voxel_texture_array_indices;
struct voxel_geometry_point { GLfloat x, y, z, meta; };

std::vector<voxel_geometry_point> voxel_point_cache;

struct voxel_point {

	uint16_t id;
	btMotionState *motion_state;
	btRigidBody *body;
};

std::vector<voxel_point> voxel_point_region(100 * 100 * 100);

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
	const glm::ivec2 expected_size { 16, 16 };
	if (texture_voxel_array) glDeleteTextures(1, &texture_voxel_array);
	voxel_texture_array_indices.clear();
	int num_files = 0;
	for (auto &i : std::filesystem::directory_iterator("texture\\face")) {
		if (!std::filesystem::is_regular_file(i)) continue;
		num_files++;
	}
	GLfloat max_anisotropic_filter_value;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic_filter_value); 
	glGenTextures(1, &texture_voxel_array);
	assert(texture_voxel_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture_voxel_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, expected_size.x, expected_size.y, num_files, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
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
		if (w == expected_size.x && h == expected_size.y) {
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
	glTextureParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filter_value);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void unload_objects() {
	for (auto &object : objects) {
		if (object.second.texture) glDeleteTextures(1, &object.second.texture);
	}
	objects.clear();
}

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

void update_voxel_world() {
	std::cout << rang::bg::yellow << rang::fg::black;
	std::cout << "Uploading voxels to GPU...";
	std::cout << rang::bg::reset << rang::fg::reset << std::endl;
	for (int z = 0; z < 100; z++) {
		for (int y = 0; y < 100; y++) {
			for (int x = 0; x < 100; x++) {
				const int index = (z * 10000) + (y * 100) + x;
				if (z == 50) voxel_point_region[index].id = 1;
			}
		}
	}
	voxel_point_cache.clear();
	for (int z = 0; z < 100; z++) {
		for (int y = 0; y < 100; y++) {
			for (int x = 0; x < 100; x++) {
				const int index = (z * 10000) + (y * 100) + x;
				if (!voxel_point_region[index].id) continue;
				int face_data = 63;
				const int neighbor_index_left = index - 1;
				const int neighbor_index_right = index + 1;
				const int neighbor_index_below = index - 10000;
				const int neighbor_index_above = index + 10000;
				const int neighbor_index_ahead = index + 100;
				const int neighbor_index_behind = index - 100;
				if (z == 99 || voxel_point_region[neighbor_index_above].id != 0) face_data -= 32;
				if (z == 0 || voxel_point_region[neighbor_index_below].id != 0) face_data -= 16;
				if (x == 99 || voxel_point_region[neighbor_index_right].id != 0) face_data -= 8;
				if (x == 0 || voxel_point_region[neighbor_index_left].id != 0) face_data -= 4;
				if (y == 99 || voxel_point_region[neighbor_index_ahead].id != 0) face_data -= 2;
				if (y == 0 || voxel_point_region[neighbor_index_behind].id != 0) face_data -= 1;
				if (!face_data) continue;
				voxel_point_cache.push_back({
					x - 50.f,
					y - 50.f,
					z - 50.f,
					static_cast<float>(face_data)
				});
				static btBoxShape half_voxel_box_shape(btVector3(1, 1, 1));
				btTransform transform;
				transform.setIdentity();
				transform.setOrigin(to_physics_world(-50.0f + glm::vec3(x, y, z)));
				voxel_point_region[index].motion_state = new btDefaultMotionState(transform);
				voxel_point_region[index].body = new btRigidBody(0, voxel_point_region[index].motion_state, &half_voxel_box_shape);
				dynamics_world->addRigidBody(voxel_point_region[index].body, 1, 2);
				// std::cout << rang::fg::yellow << transform.getOrigin().x() << " " << transform.getOrigin().y() << " " << transform.getOrigin().z() << rang::fg::reset << std::endl;
			}
		}
	}
	glBindVertexArray(voxel_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, voxel_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(voxel_geometry_point) * voxel_point_cache.size(), voxel_point_cache.data(), GL_STREAM_DRAW);
}

void render_voxel_world() {
	static bool first = true;
	if (first) {
		update_voxel_world();
		first = false;
	}
	auto model = glm::identity<glm::mat4>();
	auto total_transform = cw::pov::projection_matrix * cw::pov::view_matrix * model;
	glUseProgram(gpu_programs["main"]);
	GLint world_transform_location = glGetUniformLocation(gpu_programs["main"], "world_transform");
	GLint total_transform_location = glGetUniformLocation(gpu_programs["main"], "total_transform");
	if (world_transform_location != -1) glUniformMatrix4fv(world_transform_location, 1, GL_FALSE, glm::value_ptr(model));
	if (total_transform_location != -1) glUniformMatrix4fv(total_transform_location, 1, GL_FALSE, glm::value_ptr(total_transform));
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



/*

	Loop
	Fixed Update

*/

void on_fixed_step(double delta) {
	player_location_interpolation_pair[0] = player_location_interpolation_pair[1];
	assert(dynamics_world->stepSimulation(delta, 0, 0) == 1);
	player_location_interpolation_pair[1] = from_physics_world(player_rigid_body->getWorldTransform().getOrigin());
	auto ray_from = player_rigid_body->getWorldTransform().getOrigin();
	auto ray_to = player_rigid_body->getWorldTransform().getOrigin() - btVector3(0, 3, 0);
	btCollisionWorld::ClosestRayResultCallback ray_cb(ray_from, ray_to);
	ray_cb.m_collisionFilterGroup = 2;
	dynamics_world->rayTest(ray_from, ray_to, ray_cb);
	if (ray_cb.hasHit()) {
		glm::vec2 bipedal_movement { 0, 0 };
		if (player_binary_input_left) bipedal_movement.x -= 1;
		if (player_binary_input_right) bipedal_movement.x += 1;
		if (player_binary_input_forward) bipedal_movement.y -= 1;
		if (player_binary_input_backward) bipedal_movement.y += 1;
		if (bipedal_movement.x || bipedal_movement.y) {
			bipedal_movement = glm::normalize(bipedal_movement) * 4.0f;
			bipedal_movement = glm::vec4(bipedal_movement, 0, 0) * glm::rotate(glm::radians(-cw::pov::orientation.x), glm::vec3(0, 0, 1));
		}
		float diff = 3 - (ray_cb.m_rayFromWorld.y() - ray_cb.m_hitPointWorld.y());
		auto old_velocity = player_rigid_body->getLinearVelocity();
		player_rigid_body->setLinearVelocity(btVector3(bipedal_movement.x, glm::mix(old_velocity.y(), diff, 0.9f), bipedal_movement.y));
	} else player_rigid_body->setLinearFactor(btVector3(1, 1, 1));
}

void on_update(double delta, double interpolation) {
	static bool first = true;
	if (first) {
		initialize_player();
		first = false;
	}
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
	if (enable_physics_interpolation) cw::pov::eye = glm::mix(player_location_interpolation_pair[0], player_location_interpolation_pair[1], 1.0 - interpolation_delta);
	else cw::pov::eye = player_location_interpolation_pair[1];
	cw::pov::look = { 0, 1, 0 };
	if (cw::pov::orientation.y < -45.0f) cw::pov::orientation.y = -45.0f;
	if (cw::pov::orientation.y > 45.0f) cw::pov::orientation.y = 45.0f;
	cw::pov::look = glm::vec4(cw::pov::look, 1) * glm::rotate(glm::radians(cw::pov::orientation.y), glm::vec3(1, 0, 0));
	cw::pov::look = glm::vec4(cw::pov::look, 1) * glm::rotate(glm::radians(cw::pov::orientation.x), glm::vec3(0, 0, 1));
	cw::pov::center = cw::pov::eye + cw::pov::look;
	cw::pov::aspect = render_target_aspect;
	cw::pov::view_matrix = glm::lookAt(cw::pov::eye, cw::pov::center, cw::pov::up);
	cw::pov::projection_matrix = glm::perspective<float>(cw::pov::field_of_view, cw::pov::aspect, cw::pov::near_plane_distance, cw::pov::far_plane_distance);
	render_voxel_world();
	{
		auto model =
			glm::translate(glm::identity<glm::mat4>(), glm::vec3(0, 0, 0.5f))
			* glm::rotate(glm::radians(90.0f), glm::vec3(1, 0, 0))
			* glm::scale(glm::vec3(1.0f));
		auto total_transform = cw::pov::projection_matrix * cw::pov::view_matrix * model;
		glUseProgram(gpu_programs["mesh"]);
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
		glUniform2f(glGetUniformLocation(gpu_programs["mesh"], "material_identifier"), 0, 1000);
		glBindVertexArray(objects["fireowl-breaker"].vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, objects["fireowl-breaker"].vertex_buffer);
		glDrawArrays(GL_TRIANGLES, 0, objects["fireowl-breaker"].num_vertices);
	}
	{
		auto model = glm::identity<glm::mat4>();
		model *= glm::translate(glm::identity<glm::mat4>(), glm::vec3(-1.5, -1.5, 0.5f));
		model *= glm::rotate(glm::radians(90.0f), glm::vec3(1, 0, 0));
		model *= glm::scale(glm::vec3(1.0f));
		auto total_transform = cw::pov::projection_matrix * cw::pov::view_matrix * model;
		glUseProgram(gpu_programs["mesh"]);
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(gpu_programs["mesh"], "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
		glUniform2f(glGetUniformLocation(gpu_programs["mesh"], "material_identifier"), 0, 1001);
		glBindVertexArray(objects["sniper"].vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, objects["sniper"].vertex_buffer);
		glDrawArrays(GL_TRIANGLES, 0, objects["sniper"].num_vertices);
	}
	{
		auto &user_pref = cfg["player"]["gun_view_preferences"]["hmg"];
		auto model = glm::identity<glm::mat4>();
		model = model * glm::translate(glm::identity<glm::mat4>(), cw::pov::eye);
		model = model * glm::rotate(glm::radians(-cw::pov::orientation.x), glm::vec3(0, 0, 1));
		model = model * glm::rotate(glm::radians(-cw::pov::orientation.y), glm::vec3(1, 0, 0));
		model = model * glm::translate(glm::fvec3(user_pref["x"], user_pref["y"], user_pref["z"]));
		model = model * glm::rotate(glm::radians(static_cast<float>(user_pref.value("angle", 0))), glm::vec3(0, 0, 1));
		model = model * glm::rotate(glm::radians(90.0f), glm::vec3(1, 0, 0));
		auto total_transform = cw::pov::projection_matrix * cw::pov::view_matrix * model;
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
	if (near_plane_location != -1) glUniform1f(near_plane_location, cw::pov::near_plane_distance);
	if (far_plane_location != -1) glUniform1f(far_plane_location, cw::pov::far_plane_distance);
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
	ImGui::Begin("Game State");
	ImGui::Text(fmt::format("Frame Delta: {}", delta).c_str());
	ImGui::Text(fmt::format("Frame Time: {}", static_cast<int>(delta * 1000.0)).c_str());
	ImGui::Text(fmt::format("Frames Per Second: {}", static_cast<int>(1.0 / delta)).c_str());
	ImGui::Separator();
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
	ImGui::Text("Engine Timings");
	ImGui::PopFont();
	ImGui::Text(fmt::format("Fixed Steps Per Second: {}", fixed_steps_per_second).c_str());
	ImGui::Text(fmt::format("Performance Frequency: {}", performance_frequency).c_str());
	ImGui::Text(fmt::format("Last Performance Counter: {}", last_performance_counter).c_str());
	ImGui::Text(fmt::format("Variable Time Delta: {}", variable_time_delta).c_str());
	ImGui::Text(fmt::format("Fixed Step Time Delta: {}", fixed_step_time_delta).c_str());
	ImGui::Text(fmt::format("Performance Counters Per Fixed Step: {}", num_performance_counters_per_fixed_step).c_str());
	ImGui::Text(fmt::format("Fixed Step Counter Remainder: {}", fixed_step_counter_remainder).c_str());
	ImGui::Text(fmt::format("Optimal Performance: {}", is_performance_optimal ? "Yes" : "No").c_str());
	ImGui::Text(fmt::format("Tick: {}", current_tick_iteration).c_str());
	ImGui::Separator();
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
	ImGui::Text("Point of View");
	ImGui::PopFont();
	ImGui::InputFloat3("Eye", &cw::pov::eye.x, 4, ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat3("Center", &cw::pov::center.x, 4, ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat3("Look", &cw::pov::look.x, 4, ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat4("Orientation", &cw::pov::orientation.x, 4, ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat("Near", &cw::pov::near_plane_distance, 0, 0, 4, ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat("Far", &cw::pov::far_plane_distance, 0, 0, 4, ImGuiInputTextFlags_ReadOnly);
	ImGui::Separator();
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
	ImGui::Text("Rendering");
	ImGui::PopFont();
	ImGui::SliderFloat("Gamma", &gamma_power, 0.5, 3);
	ImGui::Checkbox("Show Wireframe", &show_wireframe);
	ImGui::Checkbox("Enable Interpolation", &enable_physics_interpolation);
	auto &user_pref = cfg["player"]["gun_view_preferences"]["hmg"];
	glm::fvec3 offset { user_pref["x"], user_pref["y"], user_pref["z"] };
	if (ImGui::SliderFloat3("Weapon View Offset", &offset.x, -2.5, 2.5)) {
		user_pref["x"] = offset.x;
		user_pref["y"] = offset.y;
		user_pref["z"] = offset.z;
	}
	ImGui::End();
	if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
		ImGui::GetBackgroundDrawList()->AddText({ 20, render_target_size.y - 36 }, IM_COL32(0, 0, 0, 255), "Mouse Grab is On [L-CTRL]");
		ImGui::PopFont();
	} else {
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
		ImGui::GetBackgroundDrawList()->AddText({ 20, render_target_size.y - 36 }, IM_COL32(0, 0, 0, 255), "Mouse Grab is Off [L-CTRL]");
		ImGui::PopFont();
	}
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(sdl_window);
}



/*

	Entry Point

*/

void initialize_game_components() {
	std::cout << "Starting game component initialization now." << std::endl;
	initialize_physics();
	make_screen_quad();
	load_voxel_textures();
	prepare_voxel_array();
	load_objects();
	load_object_textures();
	initialize_player();
}

void shutdown_game_components() {
	std::cout << "Start game component shutdown now." << std::endl;
	shutdown_player();
	cleanup_physics();
}

