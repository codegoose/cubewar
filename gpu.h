#pragma once
#define GLEW_STATIC
#include <GL/glew.h>
#include <glm/vec2.hpp>
#include <map>
#include <string.h>

namespace cw::gpu {
	extern glm::ivec2 render_target_size;
	extern std::map<std::string, GLuint> programs;
}
