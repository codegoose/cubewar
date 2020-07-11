#pragma once

#include <map>
#include <string>
#include <glm/vec3.hpp>

namespace cw::meshes {
	struct prop {
		unsigned int vertex_array = 0;
		unsigned int vertex_buffer = 0;
		unsigned int num_vertices = 0;
		glm::vec3 aabb[2];
	};
	extern std::map<std::string, prop> props;
}
