#include <iostream>
#include <imgui.h>
#include <assert.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "sys.h"
#include "gpu.h"
#include "physics.h"
#include "pov.h"
#include "meshes.h"
#include "materials.h"
#include "sun.h"
#include "net.h"

namespace cw::core {
	void initialize();
	void shutdown();
	void on_fixed_step(const double &delta);
	void on_update(const double &delta, const double &interpolation);
	void on_relative_mouse_input(int x, int y);
	void on_deferred_render();
	void on_shadow_map_render();
	void on_imgui();
}

namespace cw::local_player {
	extern btRigidBody *rigid_body;
	extern bool binary_input[4];
	extern glm::vec2 movement_input;
	extern glm::vec3 location_interpolation_pair[2];
	extern glm::vec3 interpolated_location;
	void initialize();
	void shutdown();
}

namespace cw::voxels {
	void render();
	void render_shadow_map();
}

namespace cw::gpu {
	extern bool enable_wireframe;
	extern float saturation_power;
	extern float exposure_power;
	extern float gamma_power;
	extern float sharpening_power;
}

namespace cw::sys {
	extern bool enable_mouse_grab;
	extern float mouse_look_sensitivity;
}

void cw::core::initialize() {
	sys::enable_mouse_grab = false;
	local_player::initialize();
}

void cw::core::shutdown() {
	local_player::shutdown();
}

void cw::core::on_fixed_step(const double &delta) {
	assert(physics::dynamics_world->stepSimulation(delta, 0, 0) == 1);
	local_player::location_interpolation_pair[0] = local_player::location_interpolation_pair[1];
	local_player::location_interpolation_pair[1] = physics::from(local_player::rigid_body->getWorldTransform().getOrigin());
	auto ray_from = local_player::rigid_body->getWorldTransform().getOrigin();
	auto ray_to = local_player::rigid_body->getWorldTransform().getOrigin() - btVector3(0, 2.5f, 0);
	btCollisionWorld::ClosestRayResultCallback ray_cb(ray_from, ray_to);
	ray_cb.m_collisionFilterGroup = 2;
	physics::dynamics_world->rayTest(ray_from, ray_to, ray_cb);
	if (ray_cb.hasHit()) {
		local_player::rigid_body->setLinearFactor(btVector3(1.0f, 1.0f, 1.0f));
		local_player::movement_input = { 0, 0 };
		if (local_player::binary_input[1]) local_player::movement_input.x -= 1;
		if (local_player::binary_input[3]) local_player::movement_input.x += 1;
		if (local_player::binary_input[0]) local_player::movement_input.y += 1;
		if (local_player::binary_input[2]) local_player::movement_input.y -= 1;
		glm::vec3 bipedal_movement(local_player::movement_input, 0);
		if (local_player::movement_input.x || local_player::movement_input.y) {
			local_player::movement_input = glm::normalize(local_player::movement_input) * 5.0f;
			bipedal_movement = glm::vec4(local_player::movement_input, 0, 0) * glm::rotate(glm::radians(cw::pov::orientation.x), glm::vec3(0, 0, 1));
			bipedal_movement.y *= -1.0f;
		}
		float diff = 2.5f - (ray_cb.m_rayFromWorld.y() - ray_cb.m_hitPointWorld.y());
		diff *= 10.0f;
		auto old_velocity = local_player::rigid_body->getLinearVelocity();
		local_player::rigid_body->setLinearVelocity(btVector3(bipedal_movement.x, glm::mix(old_velocity.y(), diff, 0.99f), bipedal_movement.y));
	} else local_player::rigid_body->setLinearFactor(btVector3(0.5f, 1.0f, 0.5f));
}

void cw::core::on_update(const double &delta, const double &interpolation) {
	local_player::interpolated_location = glm::mix(local_player::location_interpolation_pair[0], local_player::location_interpolation_pair[1], interpolation);
	cw::pov::eye = local_player::interpolated_location + glm::vec3(0, 0, 0.75f);
	pov::look = { 0, 1, 0 };
	if (pov::orientation.y < -85.0f) pov::orientation.y = -85.0f;
	if (pov::orientation.y > 85.0f) pov::orientation.y = 85.0f;
	pov::look = glm::vec4(pov::look, 1) * glm::rotate(glm::radians(pov::orientation.y), glm::vec3(1, 0, 0));
	pov::look = glm::vec4(pov::look, 1) * glm::rotate(glm::radians(pov::orientation.x), glm::vec3(0, 0, 1));
	pov::center = pov::eye + pov::look;
	pov::aspect = static_cast<float>(gpu::render_target_size.x) / static_cast<float>(gpu::render_target_size.y);
	pov::view_matrix = glm::lookAt(pov::eye, pov::center, pov::up);
	pov::projection_matrix = glm::perspective(pov::field_of_view, pov::aspect, pov::near_plane_distance, pov::far_plane_distance);
	sun::shadow_view_matrix = glm::lookAt(pov::eye + (glm::normalize(glm::vec3(0.25f, 0.25f, 1.0f)) * 60.0f), pov::eye, pov::up);
	sun::shadow_projection_matrix = glm::ortho<float>(-60.0f, 60.0f, -60.0f, 60.0f, 0.0f, 120.0f);
	sun::shadow_matrix = sun::shadow_projection_matrix * sun::shadow_view_matrix;
}

void cw::core::on_relative_mouse_input(int x, int y) {
	pov::orientation.x += x * sys::mouse_look_sensitivity;
	pov::orientation.y += y * sys::mouse_look_sensitivity;
}

void cw::core::on_deferred_render() {
	voxels::render();
	auto program = gpu::programs["mesh"];
	// test
	{
		auto prop = meshes::props["future_chair_1"];
		auto model = glm::identity<glm::mat4>();
		model *= glm::translate(glm::vec3(64, 64, 20.5f));
		model *= glm::scale(glm::vec3(0.025f));
		auto total_transform = cw::pov::projection_matrix * cw::pov::view_matrix * model;
		glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program, "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(program, "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
		for (auto &part : prop.parts) {
			glBindVertexArray(part.array);
			glBindBuffer(GL_ARRAY_BUFFER, part.buffer);
			float material_id = static_cast<size_t>(std::distance(materials::registry.begin(), materials::registry.find(part.material_name)));
			glUniform2f(glGetUniformLocation(program, "material_identifier"), 0, material_id);
			glDrawArrays(GL_TRIANGLES, 0, part.num_vertices);
		}
	}
	{
		auto prop = meshes::props["weapon_launcher"];
		auto model = glm::identity<glm::mat4>();
		model *= glm::translate(glm::identity<glm::mat4>(), pov::eye);
		model *= glm::rotate(glm::radians(-pov::orientation.x), glm::vec3(0, 0, 1));
		model *= glm::rotate(glm::radians(-pov::orientation.y), glm::vec3(1, 0, 0));
		model *= glm::scale(glm::vec3(0.4f));
		model *= glm::translate(glm::vec3(0.5f, 1.5f, -1.7f));
		// model *= glm::translate(glm::vec3(local_player::movement_input.x, local_player::movement_input.y, 0) * 0.005f);
		auto total_transform = cw::pov::projection_matrix * cw::pov::view_matrix * model;
		glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program, "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(program, "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
		for (auto &part : prop.parts) {
			glBindVertexArray(part.array);
			glBindBuffer(GL_ARRAY_BUFFER, part.buffer);
			float material_id = static_cast<size_t>(std::distance(materials::registry.begin(), materials::registry.find(part.material_name)));
			glUniform2f(glGetUniformLocation(program, "material_identifier"), 0, material_id);
			glDrawArrays(GL_TRIANGLES, 0, part.num_vertices);
		}
	}
}

void cw::core::on_shadow_map_render() {
	voxels::render_shadow_map();
	//
	auto program = gpu::programs["mesh-shadow-map"];
	// test
	auto prop = meshes::props["future_chair_1"];
	auto model = glm::identity<glm::mat4>();
	model *= glm::translate(glm::vec3(64, 64, 20.5f));
	model *= glm::scale(glm::vec3(0.025f));
	auto total_transform = sun::shadow_projection_matrix * sun::shadow_view_matrix * model;
	glUseProgram(program);
	glUniformMatrix4fv(glGetUniformLocation(program, "world_transform"), 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(glGetUniformLocation(program, "total_transform"), 1, GL_FALSE, glm::value_ptr(total_transform));
	for (auto &part : prop.parts) {
		glBindVertexArray(part.array);
		glBindBuffer(GL_ARRAY_BUFFER, part.buffer);
		float material_id = static_cast<size_t>(std::distance(materials::registry.begin(), materials::registry.find(part.material_name)));
		glUniform2f(glGetUniformLocation(program, "material_identifier"), 0, material_id);
		glDrawArrays(GL_TRIANGLES, 0, part.num_vertices);
	}
}

void cw::core::on_imgui() {
	static char ip_buffer[16] = { 0 };
	static int port_buffer = 4302;
	static bool first = true;
	if (first) {
		memcpy(ip_buffer, "localhost", 9);
		first = false;
	}
	ImGui::Begin("Rendering");
	ImGui::Checkbox("Wireframe", &gpu::enable_wireframe);
	ImGui::SliderFloat("Gamma", &gpu::gamma_power, 0.1, 3);
	ImGui::SliderFloat("Exposure", &gpu::exposure_power, 0.1, 10);
	ImGui::SliderFloat("Saturation", &gpu::saturation_power, 0.0, 1.5);
	ImGui::SliderFloat("Sharpening", &gpu::sharpening_power, 0, 1);
	ImGui::End();
	ImGui::Begin("User");
	ImGui::InputFloat3("Eye", &pov::eye.x, 3, ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat2("Orientiation", &pov::orientation.x, 3, ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat3("Look", &pov::look.x, 3, ImGuiInputTextFlags_ReadOnly);
	ImGui::SliderFloat("Mouse Look Sensitivity", &sys::mouse_look_sensitivity, 0.001, 1, "%.4f");
	ImGui::End();
	ImGui::Begin("Multiplayer");
	ImGui::InputText("IP Address", ip_buffer, 15);
	ImGui::InputInt("Port", &port_buffer);
	if (ImGui::Button("Connect")) {
		net::start_connection_attempt(ip_buffer, port_buffer);
	}
	ImGui::SameLine();
	if (ImGui::Button("Host")) {
		if (port_buffer < 0 || port_buffer > std::numeric_limits<uint16_t>::max()) std::cout << "Cannot host. Specified port is out of range." << std::endl;
		else net::become_server(static_cast<uint16_t>(port_buffer));
	}
	ImGui::SameLine();
	if (ImGui::Button("Quit")) net::shutdown();
	if (net::current_state == net::state::idle) ImGui::Text("Status: Idle");
	else if (net::current_state == net::state::connecting) ImGui::Text("Status: Connecting...");
	else if (net::current_state == net::state::client) ImGui::Text("Status: Connected!");
	else if (net::current_state == net::state::server) ImGui::Text("Status: Hosting!");
	ImGui::End();
}
