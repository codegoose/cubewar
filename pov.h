#pragma once
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace cw::pov {
	extern float aspect;
	extern glm::vec3 eye;
	extern glm::vec3 center;
	extern glm::vec3 look;
	extern glm::vec2 orientation;
	extern float field_of_view;
	extern float near_plane_distance;
	extern float far_plane_distance;
	extern glm::mat4 view_matrix;
	extern glm::mat4 projection_matrix;
	extern const glm::vec3 up;
}
