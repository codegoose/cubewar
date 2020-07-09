#include <iostream>
#include <imgui.h>
#include <assert.h>
#include <glm/gtx/transform.hpp>

#include "gpu.h"
#include "physics.h"
#include "pov.h"

namespace cw::core {

	bool enable_physics_interpolation = true;

	void initialize();
	void shutdown();
	void on_fixed_step(const double &delta);
	void on_update(const double &delta, const double &interpolation);
	void on_deferred_render();
	void on_imgui();
}

namespace cw::voxels {
	void render();
}

namespace cw::gpu {
	extern bool enable_wireframe;
}

void cw::core::initialize() {

}

void cw::core::shutdown() {

}

void cw::core::on_fixed_step(const double &delta) {
	assert(physics::dynamics_world->stepSimulation(delta, 0, 0) == 1);
}

void cw::core::on_update(const double &delta, const double &interpolation) {
	float render_target_aspect = static_cast<float>(gpu::render_target_size.x) / static_cast<float>(gpu::render_target_size.y);
	pov::eye = { 4, 4, 4 };
	/*
	pov::look = { 0, 1, 0 };
	if (pov::orientation.y < -45.0f) pov::orientation.y = -45.0f;
	if (pov::orientation.y > 45.0f) pov::orientation.y = 45.0f;
	pov::look = glm::vec4(pov::look, 1) * glm::rotate(glm::radians(pov::orientation.y), glm::vec3(1, 0, 0));
	pov::look = glm::vec4(pov::look, 1) * glm::rotate(glm::radians(pov::orientation.x), glm::vec3(0, 0, 1));
	pov::center = pov::eye + pov::look;
	*/
	pov::center = { 0, 0, 0 };
	pov::aspect = render_target_aspect;
	pov::view_matrix = glm::lookAt(pov::eye, pov::center, pov::up);
	pov::projection_matrix = glm::perspective<float>(pov::field_of_view, pov::aspect, pov::near_plane_distance, pov::far_plane_distance);
}

void cw::core::on_deferred_render() {
	voxels::render();
}

void cw::core::on_imgui() {
	ImGui::Checkbox("Wireframe", &gpu::enable_wireframe);
}
