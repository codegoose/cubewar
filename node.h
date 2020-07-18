#pragma once

#include <cstdint>
#include <vector>
#include <utility>
#include <memory>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace cw {
	struct node {
		bool updated_this_frame = false;
		bool needs_local_update = false;
		bool needs_global_update = false;
		uint64_t last_update_tick = 0;
		glm::mat4 local_transform, absolute_transform;
		std::vector<std::weak_ptr<node>> children;
		std::weak_ptr<node> parent;
		glm::vec3 location, scale { 1, 1, 1 };
		glm::quat orientation;
		bool inherit_location = true;
		bool inherit_scale = true;
		bool inherit_orientation = true;
		std::pair<glm::vec3, glm::vec3> location_interpolation_pair;
		std::pair<glm::vec3, glm::vec3> scale_interpolation_pair;
		std::pair<glm::quat, glm::quat> orientation_interpolation_pair;
	};
}
