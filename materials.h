#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <glm/vec3.hpp>

namespace cw::materials {
	struct properties {
		glm::vec3 diffuse;
	};
	extern std::map<std::string, properties> registry;
}
