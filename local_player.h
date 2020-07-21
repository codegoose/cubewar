#pragma once

#include "physics.h"
#include "node.h"

namespace cw::local_player {
	extern std::shared_ptr<node> camera_proxy;
	extern btRigidBody *rigid_body;
}
