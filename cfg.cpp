#include "cfg.h"
#include "misc.h"
#include "sys.h"

#include <filesystem>
#include <iostream>

std::map<std::string, nlohmann::json> cw::cfg;

namespace cw {
	void load_cfg();
	void flush_cfg();
}


void cw::load_cfg() {
	auto path = sys::bin_path().string() + "cfg\\";
	for (auto &file : std::filesystem::directory_iterator(path)) {
		if (file.path().extension().string() != ".json") continue;
		auto section = file.path().stem().string();
		std::cout << "Loading configuration section: \"" << section << "\"" << std::endl;
		auto content = misc::read_file(file.path().string());
		if (!content) continue;
		cfg[section] = nlohmann::json::parse(content->begin(), content->end());
	}
}

void cw::flush_cfg() {
	auto path = sys::bin_path().string() + "cfg\\";
	for (auto &section : cfg) {
		std::cout << "Flushing configuration section: \"" << section.first << "\"" << std::endl;
		auto content = section.second.dump(1);
		misc::write_file(path + section.first + ".json", { content.begin(), content.end() });
	}
}
