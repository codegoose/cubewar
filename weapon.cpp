#include "weapon.h"
#include "meshes.h"
#include "gpu.h"
#include "pov.h"
#include "materials.h"

#include <algorithm>
#include <glm/matrix.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>

namespace cw::weapon {
	void render_local_player_hud_model();
}

cw::weapon::id cw::weapon::local_player_equipped = cw::weapon::id::null;

void cw::weapon::render_local_player_hud_model() {
	auto program = gpu::programs["mesh"];
	auto prop = meshes::props["weapon_pdg"];
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
