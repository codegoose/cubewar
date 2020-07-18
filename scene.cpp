#include "scene.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

std::vector<std::weak_ptr<cw::node>> cw::scene::nodes;

namespace cw::scene {
	void update(const double &interpolation_delta);
	void cleanup_expired_entries();
	void calculate_node_transforms(const double &interpolation_delta);
	void walk_node_for_calculations(std::weak_ptr<node> node);
}

void cw::scene::update(const double &interpolation_delta) {
	cleanup_expired_entries();
	calculate_node_transforms(interpolation_delta);
}

void cw::scene::cleanup_expired_entries() {

}

void cw::scene::calculate_node_transforms(const double &interpolation_delta) {
	for (auto node : nodes) {
		auto ptr = node.lock();
		if (!ptr) continue;
		ptr->updated_this_frame = false;
	}
	for (auto node : nodes) walk_node_for_calculations(node);
}

void cw::scene::walk_node_for_calculations(std::weak_ptr<node> node) {
	auto ptr = node.lock();
	if (!ptr) return;
	walk_node_for_calculations(ptr->parent);
	bool must_update_children = ptr->needs_local_update || ptr->needs_global_update;
	if (ptr->needs_local_update) {
		ptr->local_transform = glm::identity<glm::mat4>();
		ptr->local_transform *= glm::translate(ptr->location);
		ptr->local_transform *= glm::scale(ptr->scale);
		ptr->local_transform *= glm::mat4(ptr->orientation);
		ptr->needs_local_update = false;
		ptr->needs_global_update = true;
	}
	if (ptr->needs_global_update) {
		if (auto parent_ptr = ptr->parent.lock(); parent_ptr) {
			ptr->absolute_transform = parent_ptr->absolute_transform * ptr->local_transform;
		} else ptr->absolute_transform = ptr->local_transform;
		ptr->needs_global_update = false;
		ptr->updated_this_frame = true;
	}
	if (must_update_children) {
		for (auto child : ptr->children) {
			auto child_ptr = child.lock();
			if (!child_ptr) continue;
			child_ptr->needs_global_update = true;
		}
		for (auto child : ptr->children) walk_node_for_calculations(child);
	}
}
