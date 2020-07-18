#pragma once

#include "node.h"

#include <vector>

namespace cw::scene {
	extern std::vector<std::weak_ptr<node>> nodes;
}
