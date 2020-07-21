#include "weapon.h"
#include "meshes.h"
#include "gpu.h"
#include "pov.h"
#include "materials.h"
#include "scene.h"
#include "local_player.h"

#include <algorithm>
#include <glm/matrix.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>

namespace cw::weapon {
	auto hud_node = std::make_shared<node>();
	void update(const double &delta);
	void render_local_player_hud_model();
}

cw::weapon::id cw::weapon::local_player_equipped = cw::weapon::id::null;

void cw::weapon::update(const double &delta) {
	auto default_gun_location = glm::vec3(0.45f, 0.7f, -0.4f);
	glm::fvec3 local_player_velocity_offset = physics::from(local_player::rigid_body->getLinearVelocity()) * local_player::camera_proxy->orientation * -0.002f;
	auto target_location = default_gun_location + local_player_velocity_offset;
	hud_node->location = glm::mix(hud_node->location, target_location, 0.035f);
	hud_node->needs_local_update = true;
}

void cw::weapon::render_local_player_hud_model() {
	static bool first = true;
	if (first) {
		hud_node->parent = local_player::camera_proxy;
		local_player::camera_proxy->children.push_back(hud_node);
		hud_node->scale = { 0.4f, 0.2f, 0.4f };
		first = false;
	}
	auto program = gpu::programs["mesh"];
	auto prop = meshes::props["weapon_pdg"];
	auto model = hud_node->absolute_transform;
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
