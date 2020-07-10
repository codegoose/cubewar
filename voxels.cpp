#include <vector>
#include <iostream>

#include "voxels.h"
#include "pov.h"
#include "physics.h"
#include "gpu.h"
#include "textures.h"

#include <glm/matrix.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace cw::voxels {

	GLuint primary_vertex_array = 0;
	GLuint primary_vertex_buffer = 0;

	struct geometry_point {
		GLfloat x;
		GLfloat y;
		GLfloat z;
		GLfloat meta;
		GLfloat matid;
	};

	std::vector<geometry_point> culled_point_cache;

	struct voxel_point {

		uint16_t id = static_cast<uint16_t>(id::null);
		btMotionState *motion_state = 0;
		btRigidBody *body = 0;
	};

	std::vector<voxel_point> total_point_region(100 * 100 * 100);

	void prepare_gpu_buffers();
	void update_gpu_buffers();
	void render();
}

void cw::voxels::prepare_gpu_buffers() {
	glGenVertexArrays(1, &primary_vertex_array);
	assert(primary_vertex_array);
	glGenBuffers(1, &primary_vertex_buffer);
	assert(primary_vertex_buffer);
	glBindVertexArray(primary_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, primary_vertex_buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_point), 0);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(geometry_point), (void *)(sizeof(GLfloat) * 3));
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(geometry_point), (void *)(sizeof(GLfloat) * 4));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
}

void cw::voxels::update_gpu_buffers() {
	std::cout << "Uploading voxels to GPU..." << std::endl;
	for (int z = 0; z < 100; z++) {
		for (int y = 0; y < 100; y++) {
			for (int x = 0; x < 100; x++) {
				const int index = (z * 10000) + (y * 100) + x;
				if (z == 50 && x >= 48 && x <= 52 && y >= 48 && y <= 52) total_point_region[index].id = static_cast<uint16_t>(id::stone);
				if (z >= 51 && z <= 52 && x >= 49 && x <= 51 && y >= 49 && y <= 51) total_point_region[index].id = static_cast<uint16_t>(id::sandstone);
				if (z <= 49) total_point_region[index].id = static_cast<uint16_t>(id::sand);
			}
		}
	}
	culled_point_cache.clear();
	for (int z = 0; z < 100; z++) {
		for (int y = 0; y < 100; y++) {
			for (int x = 0; x < 100; x++) {
				const int index = (z * 10000) + (y * 100) + x;
				if (!total_point_region[index].id) continue;
				int face_data = 63;
				const int neighbor_index_left = index - 1;
				const int neighbor_index_right = index + 1;
				const int neighbor_index_below = index - 10000;
				const int neighbor_index_above = index + 10000;
				const int neighbor_index_ahead = index + 100;
				const int neighbor_index_behind = index - 100;
				if (z == 99 || total_point_region[neighbor_index_above].id != 0) face_data -= 32;
				if (z == 0 || total_point_region[neighbor_index_below].id != 0) face_data -= 16;
				if (x == 99 || total_point_region[neighbor_index_right].id != 0) face_data -= 8;
				if (x == 0 || total_point_region[neighbor_index_left].id != 0) face_data -= 4;
				if (y == 99 || total_point_region[neighbor_index_ahead].id != 0) face_data -= 2;
				if (y == 0 || total_point_region[neighbor_index_behind].id != 0) face_data -= 1;
				if (!face_data) continue;
				culled_point_cache.push_back({
					static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
					static_cast<float>(face_data), static_cast<float>(total_point_region[index].id)
				});
				static btBoxShape half_voxel_box_shape(btVector3(0.5, 0.5, 0.5));
				btTransform transform;
				transform.setIdentity();
				transform.setOrigin(physics::to(glm::vec3(x, y, z)));
				total_point_region[index].motion_state = new btDefaultMotionState(transform);
				total_point_region[index].body = new btRigidBody(0, total_point_region[index].motion_state, &half_voxel_box_shape);
				physics::dynamics_world->addRigidBody(total_point_region[index].body, 1, 2);
			}
		}
	}
	glBindVertexArray(primary_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, primary_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(geometry_point) * culled_point_cache.size(), culled_point_cache.data(), GL_STREAM_DRAW);
}

void cw::voxels::render() {
	static bool first = true;
	if (first) {
		prepare_gpu_buffers();
		update_gpu_buffers();
		first = false;
	}
	auto model = glm::identity<glm::mat4>();
	auto total_transform = cw::pov::projection_matrix * cw::pov::view_matrix * model;
	glUseProgram(gpu::programs["voxels"]);
	GLint world_transform_location = glGetUniformLocation(gpu::programs["voxels"], "world_transform");
	GLint total_transform_location = glGetUniformLocation(gpu::programs["voxels"], "total_transform");
	if (world_transform_location != -1) glUniformMatrix4fv(world_transform_location, 1, GL_FALSE, glm::value_ptr(model));
	if (total_transform_location != -1) glUniformMatrix4fv(total_transform_location, 1, GL_FALSE, glm::value_ptr(total_transform));
	glBindVertexArray(primary_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, primary_vertex_buffer);
	glDrawArrays(GL_POINTS, 0, culled_point_cache.size());
}
