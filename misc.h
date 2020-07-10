#pragma once
#include <optional>
#include <vector>
#include <string>
#include <map>
#include <filesystem>

namespace cw::misc {
	std::optional<std::vector<char>> read_file(const std::filesystem::path &path);
	bool write_file(const std::filesystem::path &path, const std::vector<char> &data);
	std::optional<std::map<std::string, std::vector<std::string>>> map_file_names_and_extensions(const std::filesystem::path &path);
}