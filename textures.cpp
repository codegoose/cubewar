#include "gpu.h"
#include "sys.h"
#include "misc.h"

#include <vector>
#include <utility>
#include <stb_image.h>
#include <iostream>
#include <assert.h>
#include <glm/vec2.hpp>
#include <fmt/format.h>

namespace cw::textures {

	GLuint voxel_array = 0;
	GLuint x256_array = 0;
	GLuint x512_array = 0;

	std::map<std::string, int> voxel_indices;
	std::map<std::string, int> x256_indices;
	std::map<std::string, int> x512_indices;

	void load_all();
	void load_voxel();
	void load_general();
	void print_debug_info();
}

void cw::textures::load_all() {
	load_voxel();
	load_general();
	print_debug_info();
}

void cw::textures::load_voxel() {
	const int expected_size = 32;
	if (voxel_array) glDeleteTextures(1, &voxel_array);
	voxel_indices.clear();
	int num_files = 0;
	for (auto &i : std::filesystem::directory_iterator(sys::bin_path().string() + "texture\\face")) {
		if (!std::filesystem::is_regular_file(i)) continue;
		num_files++;
	}
	glGenTextures(1, &voxel_array);
	assert(voxel_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, voxel_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, expected_size, expected_size, num_files, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	int index = 0;
	for (auto &i : std::filesystem::directory_iterator(sys::bin_path().string() + "texture\\face")) {
		if (!std::filesystem::is_regular_file(i)) continue;
		auto content = misc::read_file(i.path());
		if (!content) continue;
		int w, h, channels;
		unsigned char *image = stbi_load_from_memory(reinterpret_cast<unsigned char *>(content->data()), content->size(), &w, &h, &channels, STBI_rgb);
		if (!image) {
			std::cout << "Unable to resolve file contents to an image: \"" << i.path().string() << "\"" << std::endl;
			continue;
		}
		if (w == expected_size && h == expected_size) {
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index, w, h, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
			voxel_indices[i.path().stem().string()] = index;
			index++;
		} else std::cout << "Skipped loading image due to invalid size: \"" << i.path().string() << "\"" << std::endl;
		stbi_image_free(image);
		num_files--;
		assert(num_files >= 0);
	}
	//glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	//GLfloat max_anisotropic_filter_value;
	//glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic_filter_value);
	//glTextureParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic_filter_value);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void cw::textures::load_general() {
	// std::cout << "Beginning x256/x512 object texture processing now." << std::endl;
	auto items = misc::map_file_names_and_extensions(sys::bin_path().string() + "object");
	if (!items) return;
	std::map<std::string, std::pair<std::vector<char>, glm::ivec2>> textures_256, textures_512;
	for (auto &item : *items) {
		auto texture_path = fmt::format("{}object\\{}.png", sys::bin_path().string(), item.first);
		auto texture_file_contents = misc::read_file(texture_path);
		if (!texture_file_contents) continue;
		int w, h, channels;
		unsigned char *image = stbi_load_from_memory(
			reinterpret_cast<unsigned char *>(texture_file_contents->data()),
			texture_file_contents->size(),
			&w, &h, &channels, STBI_rgb
		);
		if (!image) {
			std::cout << "Unable to recognize file \"" << texture_path << "\" as an image." << std::endl;
			continue;
		}
		if (w == 256 && h == 256) {
			textures_256[item.first].first.resize(w * h * 3);
			textures_256[item.first].second = { w, h };
			memcpy(textures_256[item.first].first.data(), image, w * h * 3);
		} else if (w == 512 && h == 512) {
			textures_512[item.first].first.resize(w * h * 3);
			textures_512[item.first].second = { w, h };
			memcpy(textures_512[item.first].first.data(), image, w * h * 3);
		} else {
			std::cout << "Texture within \"" << texture_path << "\" is an unsupported size. (" << w << " by " << h << ")" << std::endl;
		}
		stbi_image_free(image);
	}
	if (x256_array) glDeleteTextures(1, &x256_array);
	if (x512_array) glDeleteTextures(1, &x512_array);
	glGenTextures(1, &x256_array);
	assert(x256_array);
	glGenTextures(1, &x512_array);
	assert(x512_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, x256_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 256, 256, textures_256.size(), 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLint z_offset = 0;
	for (auto &image : textures_256) {
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0,
			z_offset, image.second.second.x, image.second.second.y, 1,
			GL_RGB, GL_UNSIGNED_BYTE, image.second.first.data());
		x256_indices[image.first] = z_offset;
		z_offset++;
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, x512_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 512, 512, textures_512.size(), 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	z_offset = 0;
	for (auto &image : textures_512) {
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0,
			z_offset, image.second.second.x, image.second.second.y, 1,
			GL_RGB, GL_UNSIGNED_BYTE, image.second.first.data());
		x512_indices[image.first] = z_offset;
		z_offset++;
	}
}

void cw::textures::print_debug_info() {
	std::cout << "Loaded " << voxel_indices.size() << " voxel, " << x256_indices.size() << " x256, " << x512_indices.size() << " x512 textures." << std::endl;
	for (auto &e : voxel_indices) std::cout << " voxel[" << e.second << "] <- " << e.first << std::endl;
	for (auto &e : x256_indices) std::cout << " x256[" << e.second << "] <- " << e.first << std::endl;
	for (auto &e : x512_indices) std::cout << " x512[" << e.second << "] <- " << e.first << std::endl;
}
