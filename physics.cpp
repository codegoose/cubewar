#include "physics.h"

namespace cw::physics {
	btDefaultCollisionConfiguration *collision_configuration = 0;
	btCollisionDispatcher *collision_dispatcher = 0;
	btBroadphaseInterface *broadphase_interface = 0;
	btSequentialImpulseConstraintSolver *constraint_solver = 0;
	void initialize();
	void shutdown();
}

btDiscreteDynamicsWorld *cw::physics::dynamics_world = 0;

void cw::physics::initialize() {
	shutdown();
	collision_configuration = new btDefaultCollisionConfiguration;
	collision_dispatcher = new btCollisionDispatcher(collision_configuration);
	broadphase_interface = new btDbvtBroadphase;
	constraint_solver = new btSequentialImpulseConstraintSolver;
	dynamics_world = new btDiscreteDynamicsWorld(collision_dispatcher, broadphase_interface, constraint_solver, collision_configuration);
	dynamics_world->setGravity({ 0, -10, 0 });
}

void cw::physics::shutdown() {
	if (dynamics_world) delete dynamics_world;
	if (constraint_solver) delete constraint_solver;
	if (broadphase_interface) delete broadphase_interface;
	if (collision_dispatcher) delete collision_dispatcher;
	if (collision_configuration) delete collision_configuration;
	dynamics_world = 0;
	constraint_solver = 0;
	broadphase_interface = 0;
	collision_dispatcher = 0;
	collision_configuration = 0;
}

glm::vec3 cw::physics::from(const btVector3 &in) {
	return { in.x(), -in.z(), in.y() };
}

btVector3 cw::physics::to(const glm::vec3 &in) {
	return { in.x, in.z, -in.y };
}

glm::quat cw::physics::from(const btQuaternion &in) {
	glm::quat q;
	q.w = in.w();
	q.x = in.x();
	q.y = -in.z();
	q.z = in.y();
	return q;
}

btQuaternion cw::physics::to(const glm::quat &in) {
	btQuaternion q;
	q.setW(in.x);
	q.setX(in.z);
	q.setY(-in.y);
	q.setZ(in.w);
	return q;
}