#pragma once

namespace cw::weapon {
	enum class id {
		null,
		personal_defense_gun,
		grenade_launcher
	};
	extern id local_player_equipped;
}
