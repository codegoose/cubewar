#include "misc.h"
#include <fstream>
#include <iostream>

std::optional<std::vector<char>> cw::misc::read_file(const std::filesystem::path &path) {
	std::ifstream in(path.string(), std::ios::binary);
	if (!in.is_open()) {
		std::cout << "Failed to open file for reading: " << path.string() << std::endl;
		return std::nullopt;
	}
	std::vector<char> content(std::filesystem::file_size(path));
	if (!content.size()) return std::vector<char>();
	in.read(content.data(), content.size());
	if (static_cast<size_t>(in.tellg()) == content.size()) {
		std::cout << "File contents loaded: \"" << path.string() << "\" (" << content.size() << " bytes)" << std::endl;
		return content;
	}
	std::cout << "Unable to read entire file contents: \"" << path.string() << "\"." << std::endl;
	return std::nullopt;
}

std::optional<std::map<std::string, std::vector<std::string>>> cw::misc::map_file_names_and_extensions(const std::filesystem::path &path) {
	if (!std::filesystem::exists(path)) return std::nullopt;
	if (!std::filesystem::is_directory(path)) return std::nullopt;
	std::map<std::string, std::vector<std::string>> map;
	for (auto &i : std::filesystem::directory_iterator(path)) {
		if (!i.path().has_extension()) continue;
		map[i.path().stem().string()].push_back(i.path().extension().string());
	}
	return map;
}
