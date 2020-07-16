#include "net.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <assert.h>

ENetHost *cw::net::local_host = 0;
cw::net::state cw::net::current_state = cw::net::state::idle;

namespace cw::net {
	std::vector<ENetPeer *> active_peers;
}

void cw::net::become_server(uint16_t port) {
	shutdown();
	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = port;
	local_host = enet_host_create(&address, 32, 2, 0, 0);
	if (!local_host) {
		std::cout << "Unable to create local host. Perhaps that port is already in use?" << std::endl;
		return;
	}
	std::cout << "Listening for connections on port " << port << "." << std::endl;
	current_state = state::server;

}

void cw::net::start_connection_attempt(std::string ip_address, uint16_t port) {
	shutdown();
	local_host = enet_host_create(0, 1, 2, 0, 0);
	assert(local_host);
	ENetAddress address;
	if (enet_address_set_host(&address, ip_address.c_str()) != 0) {
		std::cout << "Connection attempt cancelled; unable to resolve host name." << std::endl;
		shutdown();
		return;
	}
	address.port = port;
	if (!enet_host_connect(local_host, &address, 2, 0)) {
		std::cout << "Connection attempt cancelled; no space available for more peers." << std::endl;
		shutdown();
		return;
	}
	current_state = state::connecting;
}

void cw::net::shutdown() {
	if (local_host) {
		for (auto peer : active_peers) enet_peer_disconnect(peer, 0);
		enet_host_flush(local_host);
		enet_host_destroy(local_host);
		std::cout << "Destroyed local network host." << std::endl;
		local_host = 0;
	}
	active_peers.clear();
	current_state = state::idle;
}

void cw::net::process() {
	if (!local_host) return;
	ENetEvent net_event;
	while (enet_host_service(local_host, &net_event, 0) != 0) {
		if (net_event.type == ENET_EVENT_TYPE_CONNECT) {
			active_peers.push_back(net_event.peer);
			if (current_state == state::connecting) {
				std::cout << "Connection to server established. Transitioning to client mode." << std::endl;
				current_state = state::client;
			} else {
				std::cout << "Peer connected. ";
				if (!active_peers.size()) std::cout << "No peers are connected." << std::endl;
				else std::cout << active_peers.size() << " peer" << (active_peers.size() == 1 ? " is " : "s are ") << "connected." << std::endl;
			}
		} else if (net_event.type == ENET_EVENT_TYPE_DISCONNECT) {
			enet_peer_reset(net_event.peer);
			active_peers.erase(std::remove(active_peers.begin(), active_peers.end(), net_event.peer), active_peers.end());
			if (current_state == state::client) {
				std::cout << "Connection to the server has been disconnected." << std::endl;
				shutdown();
				return;
			} else if (current_state == state::connecting) {
				std::cout << "Attempt to connect to the server has failed." << std::endl;
				shutdown();
				return;
			} else {
				std::cout << "Peer disconnected. ";
				if (!active_peers.size()) std::cout << "No peers are connected." << std::endl;
				else std::cout << active_peers.size() << " peer" << (active_peers.size() == 1 ? " is " : "s are ") << "connected." << std::endl;
			}
		}
	}
}
