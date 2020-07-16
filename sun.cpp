#include "sun.h"

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

glm::vec3 cw::sun::direction;
glm::mat4 cw::sun::shadow_projection_matrix;
glm::mat4 cw::sun::shadow_view_matrix;
glm::mat4 cw::sun::shadow_matrix;
