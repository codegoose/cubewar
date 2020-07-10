#include <iostream>
#include <imgui.h>
#include <assert.h>
#include <glm/gtx/transform.hpp>

#include "sys.h"
#include "gpu.h"
#include "physics.h"
#include "pov.h"

namespace cw::core {
	void initialize();
	void shutdown();
	void on_fixed_step(const double &delta);
	void on_update(const double &delta, const double &interpolation);
	void on_relative_mouse_input(int x, int y);
	void on_deferred_render();
	void on_imgui();
}

namespace cw::local_player {
	extern btRigidBody *rigid_body;
	extern bool binary_input[4];
	extern glm::vec3 movement_input;
	extern glm::vec3 location_interpolation_pair[2];
	extern glm::vec3 interpolated_location;
	void initialize();
	void shutdown();
}

namespace cw::voxels {
	void render();
}

namespace cw::gpu {
	extern bool enable_wireframe;
}

void cw::core::initialize() {
	sys::enable_mouse_grab = true;
	local_player::initialize();
}

void cw::core::shutdown() {
	local_player::shutdown();
}

void cw::core::on_fixed_step(const double &delta) {
	assert(physics::dynamics_world->stepSimulation(delta, 0, 0) == 1);
	local_player::location_interpolation_pair[0] = local_player::location_interpolation_pair[1];
	local_player::location_interpolation_pair[1] = physics::from(local_player::rigid_body->getWorldTransform().getOrigin());
	/*
	auto ray_from = player_rigid_body->getWorldTransform().getOrigin();
	auto ray_to = player_rigid_body->getWorldTransform().getOrigin() - btVector3(0, 3, 0);
	btCollisionWorld::ClosestRayResultCallback ray_cb(ray_from, ray_to);
	ray_cb.m_collisionFilterGroup = 2;
	dynamics_world->rayTest(ray_from, ray_to, ray_cb);
	if (ray_cb.hasHit()) {
		glm::vec2 bipedal_movement { 0, 0 };
		if (player_binary_input_left) bipedal_movement.x -= 1;
		if (player_binary_input_right) bipedal_movement.x += 1;
		if (player_binary_input_forward) bipedal_movement.y -= 1;
		if (player_binary_input_backward) bipedal_movement.y += 1;
		if (bipedal_movement.x || bipedal_movement.y) {
			bipedal_movement = glm::normalize(bipedal_movement) * 4.0f;
			bipedal_movement = glm::vec4(bipedal_movement, 0, 0) * glm::rotate(glm::radians(-cw::pov::orientation.x), glm::vec3(0, 0, 1));
		}
		float diff = 3 - (ray_cb.m_rayFromWorld.y() - ray_cb.m_hitPointWorld.y());
		auto old_velocity = player_rigid_body->getLinearVelocity();
		player_rigid_body->setLinearVelocity(btVector3(bipedal_movement.x, glm::mix(old_velocity.y(), diff, 0.9f), bipedal_movement.y));
	} else player_rigid_body->setLinearFactor(btVector3(1, 1, 1));
	*/
}

void cw::core::on_update(const double &delta, const double &interpolation) {
	cw::pov::eye = glm::mix(local_player::location_interpolation_pair[0], local_player::location_interpolation_pair[1], interpolation);
	pov::look = { 0, 1, 0 };
	if (pov::orientation.y < -45.0f) pov::orientation.y = -45.0f;
	if (pov::orientation.y > 45.0f) pov::orientation.y = 45.0f;
	pov::look = glm::vec4(pov::look, 1) * glm::rotate(glm::radians(pov::orientation.y), glm::vec3(1, 0, 0));
	pov::look = glm::vec4(pov::look, 1) * glm::rotate(glm::radians(pov::orientation.x), glm::vec3(0, 0, 1));
	pov::center = pov::eye + pov::look;
	pov::aspect = static_cast<float>(gpu::render_target_size.x) / static_cast<float>(gpu::render_target_size.y);
	pov::view_matrix = glm::lookAt(pov::eye, pov::center, pov::up);
	pov::projection_matrix = glm::perspective<float>(pov::field_of_view, pov::aspect, pov::near_plane_distance, pov::far_plane_distance);
}

void cw::core::on_relative_mouse_input(int x, int y) {
	pov::orientation.x += x * 0.1f;
	pov::orientation.y += y * 0.1f;
}

void cw::core::on_deferred_render() {
	voxels::render();
}

void cw::core::on_imgui() {
	ImGui::Begin("Rendering");
	ImGui::Checkbox("Wireframe", &gpu::enable_wireframe);
	ImGui::End();
}
