#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace cw::sun {
	extern glm::vec3 direction;
	extern glm::mat4 shadow_projection_matrix;
	extern glm::mat4 shadow_view_matrix;
	extern glm::mat4 shadow_matrix;
}
