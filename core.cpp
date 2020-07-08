#include <iostream>
#define GLEW_STATIC
#include <gl/glew.h>
#include <imgui.h>
#include <assert.h>

#include "physics.h"

namespace cw::core {
	void initialize();
	void shutdown();
	void on_fixed_step(const double &delta);
	void on_update(const double &delta, const double &interpolation);
	void on_imgui();
}

void cw::core::initialize() {

}

void cw::core::shutdown() {

}

void cw::core::on_fixed_step(const double &delta) {
	assert(physics::dynamics_world->stepSimulation(delta, 0, 0) == 1);
}

void cw::core::on_update(const double &delta, const double &interpolation) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.5, 0.4, 0.3, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

}

void cw::core::on_imgui() {
	ImGui::Begin("Game State");
	ImGui::Text("This is a test!");
	ImGui::End();
}