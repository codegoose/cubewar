#include "physics.h"

#include <glm/vec3.hpp>

namespace cw::local_player {
	btMotionState *motion_state = 0;
	btCollisionShape *collision_shape = 0;
	btRigidBody *rigid_body = 0;
	bool binary_input[4];
	glm::vec3 movement_input;
	glm::vec3 location_interpolation_pair[2];
	glm::vec3 interpolated_location;
	void initialize();
	void shutdown();
}

void cw::local_player::initialize() {
	shutdown();
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(physics::to(glm::vec3(64, 64, 22)));
	motion_state = new btDefaultMotionState(transform);
	collision_shape = new btCapsuleShape(0.4f, 2.0f);
	btVector3 local_inertia;
	collision_shape->calculateLocalInertia(1, local_inertia);
	rigid_body = new btRigidBody(1, motion_state, collision_shape, local_inertia);
	rigid_body->setAngularFactor(0);
	rigid_body->setActivationState(DISABLE_DEACTIVATION);
	physics::dynamics_world->addRigidBody(rigid_body, 2, 1);

}

void cw::local_player::shutdown() {
	if (rigid_body) {
		physics::dynamics_world->removeRigidBody(rigid_body);
		delete rigid_body;
		rigid_body = 0;
	}
	if (collision_shape) {
		delete collision_shape;
		collision_shape = 0;
	}
	if (motion_state) {
		delete motion_state;
		motion_state = 0;
	}
}
