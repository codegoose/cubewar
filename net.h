#pragma once

#include <cstdint>
#include <string>
#include <enet/enet.h>

namespace cw::net {
	enum class state {
		idle,
		server,
		client,
		connecting
	};
	extern ENetHost *local_host;
	extern state current_state;
	void become_server(uint16_t port);
	void start_connection_attempt(std::string ip_address, uint16_t port);
	void shutdown();
	void process();
}
