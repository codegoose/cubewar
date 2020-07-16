#include <vector>
#include <iostream>

#include "voxels.h"
#include "pov.h"
#include "physics.h"
#include "gpu.h"
#include "textures.h"
#include "sys.h"
#include "misc.h"
#include "simplex.h"
#include "materials.h"
#include "sun.h"

#include <stb_image.h>
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

	const int chunk_size = 128;
	std::vector<voxel_point> total_point_region(chunk_size * chunk_size * chunk_size);

	void prepare_gpu_buffers();
	void update_gpu_buffers();
	void render();
	void render_shadow_map();
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
	for (int z = 0; z < chunk_size; z++) {
		for (int y = 0; y < chunk_size; y++) {
			for (int x = 0; x < chunk_size; x++) {
				const int index = (z * (chunk_size * chunk_size)) + (y * chunk_size) + x;
				id type = id::null;
				if (z <= 20 || (x > 10 && x < 20 && y > 10 && y < 20 && z < 40 && z > 20)) {
					type = rand() % 100 < 50 ? id::thing_1 : id::thing_2;
					if (z >= 2 && simplex::noise(x * 0.02f, y * 0.01f, z * 0.01f) < 0) type = id::null;
				}
				total_point_region[index].id = static_cast<uint16_t>(type);
			}
		}
	}
	culled_point_cache.clear();
	for (int z = 0; z < chunk_size; z++) {
		for (int y = 0; y < chunk_size; y++) {
			for (int x = 0; x < chunk_size; x++) {
				const int index = (z * (chunk_size * chunk_size)) + (y * chunk_size) + x;
				if (!total_point_region[index].id) continue;
				int face_data = 63;
				const int neighbor_index_left = index - 1;
				const int neighbor_index_right = index + 1;
				const int neighbor_index_below = index - (chunk_size * chunk_size);
				const int neighbor_index_above = index + (chunk_size * chunk_size);
				const int neighbor_index_ahead = index + chunk_size;
				const int neighbor_index_behind = index - chunk_size;
				if (z == chunk_size - 1 || total_point_region[neighbor_index_above].id != 0) face_data -= 32;
				if (z == 0 || total_point_region[neighbor_index_below].id != 0) face_data -= 16;
				if (x == chunk_size - 1 || total_point_region[neighbor_index_right].id != 0) face_data -= 8;
				if (x == 0 || total_point_region[neighbor_index_left].id != 0) face_data -= 4;
				if (y == chunk_size - 1 || total_point_region[neighbor_index_ahead].id != 0) face_data -= 2;
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

void cw::voxels::render_shadow_map() {
	auto model = glm::identity<glm::mat4>();
	auto total_transform = cw::sun::shadow_projection_matrix * cw::sun::shadow_view_matrix * model;
	glUseProgram(gpu::programs["voxels-shadow-map"]);
	GLint world_transform_location = glGetUniformLocation(gpu::programs["voxels-shadow-map"], "world_transform");
	GLint total_transform_location = glGetUniformLocation(gpu::programs["voxels-shadow-map"], "total_transform");
	if (world_transform_location != -1) glUniformMatrix4fv(world_transform_location, 1, GL_FALSE, glm::value_ptr(model));
	if (total_transform_location != -1) glUniformMatrix4fv(total_transform_location, 1, GL_FALSE, glm::value_ptr(total_transform));
	glBindVertexArray(primary_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, primary_vertex_buffer);
	glDrawArrays(GL_POINTS, 0, culled_point_cache.size());
}
