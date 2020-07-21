#include "pov.h"
#include <glm/gtx/transform.hpp>

float cw::pov::aspect;
glm::vec3 cw::pov::eye;
glm::vec3 cw::pov::center;
glm::vec3 cw::pov::look;
glm::vec2 cw::pov::orientation;
float cw::pov::field_of_view = glm::radians(85.0f);
float cw::pov::near_plane_distance = 0.2f;
float cw::pov::far_plane_distance = 200.01f;
glm::mat4 cw::pov::view_matrix = glm::identity<glm::mat4>();
glm::mat4 cw::pov::projection_matrix = glm::identity<glm::mat4>();
const glm::vec3 cw::pov::up { 0, 0, 1 };
