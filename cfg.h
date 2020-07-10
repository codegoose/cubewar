#pragma once

#include <map>
#include <string.h>
#include <json.hpp>

namespace cw {
	extern std::map<std::string, nlohmann::json> cfg;
}
