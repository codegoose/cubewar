#include "misc.h"
#include "sys.h"
#include "gpu.h"
#include "meshes.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/vec2.hpp>
#include <iostream>
#include <vector>
#include <filesystem>
#include <assert.h>

namespace cw::meshes {
	struct vertex {
		glm::vec3 position, normal;
		glm::vec2 uv;
	};
	void load_all();
	void load_props();
}

std::map<std::string, cw::meshes::prop> cw::meshes::props;

void cw::meshes::load_all() {
	load_props();
}

void cw::meshes::load_props() {
	for (auto &file : std::filesystem::directory_iterator(sys::bin_path().string() + "prop")) {
		auto file_contents = misc::read_file(file.path());
		if (!file_contents) continue;
		Assimp::Importer importer;
		auto scene = importer.ReadFileFromMemory(
				file_contents->data(), file_contents->size(),
				aiProcess_FlipUVs | aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes | aiProcess_GenBoundingBoxes | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenNormals
			);
		if (!scene) {
			std::cout << "Error while processing prop: " << file.path().string() << std::endl;
			continue;
		}
		assert(scene->mNumMeshes == 1);
		prop new_prop;
		new_prop.aabb[0] = { scene->mMeshes[0]->mAABB.mMin.x, scene->mMeshes[0]->mAABB.mMin.x, scene->mMeshes[0]->mAABB.mMin.x };
		new_prop.aabb[1] = { scene->mMeshes[0]->mAABB.mMax.x, scene->mMeshes[0]->mAABB.mMax.x, scene->mMeshes[0]->mAABB.mMax.x };
		std::vector<vertex> vertices;
		for (unsigned int face_index = 0; face_index < scene->mMeshes[0]->mNumFaces; face_index++) {
			for (unsigned int i = 0; i < scene->mMeshes[0]->mFaces[face_index].mNumIndices; i++) {
				const auto position = scene->mMeshes[0]->mVertices[scene->mMeshes[0]->mFaces[face_index].mIndices[i]];
				const auto normal = scene->mMeshes[0]->mNormals[scene->mMeshes[0]->mFaces[face_index].mIndices[i]];
				const auto uv = scene->mMeshes[0]->mTextureCoords[0][scene->mMeshes[0]->mFaces[face_index].mIndices[i]];
				vertices.push_back({
					{ position.x, position.y, position.z },
					{ normal.x, normal.y, normal.z },
					{ uv.x, uv.y }
				});
			}
		}
		glGenVertexArrays(1, &new_prop.vertex_array);
		assert(new_prop.vertex_array);
		glGenBuffers(1, &new_prop.vertex_buffer);
		assert(new_prop.vertex_buffer);
		glBindVertexArray(new_prop.vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, new_prop.vertex_buffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), reinterpret_cast<void *>(sizeof(float) * 3));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), reinterpret_cast<void *>(sizeof(float) * 6));
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), vertices.data(), GL_STATIC_DRAW);
		new_prop.num_vertices = vertices.size();
		props[file.path().stem().string()] = new_prop;
		std::cout << "Loaded prop: \"" << file.path().stem().string() << std::endl;
	}
}
