#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <btBulletDynamicsCommon.h>

namespace cw::physics {
	glm::vec3 from(const btVector3 &in);
	btVector3 to(const glm::vec3 &in);
	glm::quat from(const btQuaternion &in);
	btQuaternion to(const glm::quat &in);
	extern btDiscreteDynamicsWorld *dynamics_world;
}