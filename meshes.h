#pragma once

#include <map>
#include <string>
#include <glm/vec3.hpp>

namespace cw::meshes {
	struct mesh {
		unsigned int array = 0;
		unsigned int buffer = 0;
		unsigned int num_vertices = 0;
		std::string material_name;
	};
	struct prop {
		std::vector<mesh> parts;
		glm::vec3 aabb[2];
	};
	extern std::map<std::string, prop> props;
}
