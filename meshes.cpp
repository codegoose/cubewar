#include "misc.h"
#include "sys.h"
#include "gpu.h"
#include "meshes.h"
#include "materials.h"

#include <fmt/format.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
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

namespace cw::sys::preload {
	void update();
}

std::map<std::string, cw::meshes::prop> cw::meshes::props;

void cw::meshes::load_all() {
	load_props();
}

void cw::meshes::load_props() {
	for (auto &section : std::filesystem::directory_iterator(sys::bin_path().string() + "prop")) {
		for (auto &file : std::filesystem::directory_iterator(section)) {
			sys::preload::update();
			auto file_contents = misc::read_file(file.path());
			if (!file_contents) continue;
			auto new_prop_name = section.path().stem().string() + "_" + file.path().stem().string();
			Assimp::Importer importer;
			auto scene = importer.ReadFileFromMemory(
					file_contents->data(), file_contents->size(),
					aiProcess_FlipUVs | aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes | aiProcess_GenBoundingBoxes | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate
				);
			if (!scene) {
				std::cout << "Error while processing prop: " << file.path().string() << std::endl;
				continue;
			}
			assert(scene->mNumMeshes > 0);
			prop new_prop;
			new_prop.aabb[0] = { scene->mMeshes[0]->mAABB.mMin.x, scene->mMeshes[0]->mAABB.mMin.x, scene->mMeshes[0]->mAABB.mMin.x };
			new_prop.aabb[1] = { scene->mMeshes[0]->mAABB.mMax.x, scene->mMeshes[0]->mAABB.mMax.x, scene->mMeshes[0]->mAABB.mMax.x };
			std::vector<vertex> vertices;
			for (unsigned int mesh_index = 0; mesh_index < scene->mNumMeshes; mesh_index++) {
				auto material = scene->mMaterials[scene->mMeshes[mesh_index]->mMaterialIndex];
				aiColor3D diffuse;
				assert(material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS);
				auto registered_material_name = fmt::format("{}_mesh_{}", new_prop_name, mesh_index);
				std::cout << "Registered material \"" << registered_material_name << "\"." << std::endl;
				materials::registry[registered_material_name] = {
					{ diffuse.r, diffuse.g, diffuse.b }
				};
				for (unsigned int face_index = 0; face_index < scene->mMeshes[mesh_index]->mNumFaces; face_index++) {
					for (unsigned int i = 0; i < scene->mMeshes[mesh_index]->mFaces[face_index].mNumIndices; i++) {
						const auto position = scene->mMeshes[mesh_index]->mVertices[scene->mMeshes[mesh_index]->mFaces[face_index].mIndices[i]];
						const auto normal = scene->mMeshes[mesh_index]->mNormals[scene->mMeshes[mesh_index]->mFaces[face_index].mIndices[i]];
						const auto uv = scene->mMeshes[mesh_index]->mTextureCoords[0][scene->mMeshes[mesh_index]->mFaces[face_index].mIndices[i]];
						vertices.push_back({
							{ position.x, position.y, position.z },
							{ normal.x, normal.y, normal.z },
							{ uv.x, uv.y }
						});
					}
				}
				mesh new_mesh;
				new_mesh.material_name = registered_material_name;
				glGenVertexArrays(1, &new_mesh.array);
				assert(&new_mesh.array);
				glGenBuffers(1, &new_mesh.buffer);
				assert(new_mesh.buffer);
				glBindVertexArray(new_mesh.array);
				glBindBuffer(GL_ARRAY_BUFFER, new_mesh.buffer);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), reinterpret_cast<void *>(sizeof(float) * 3));
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), reinterpret_cast<void *>(sizeof(float) * 6));
				glEnableVertexAttribArray(0);
				glEnableVertexAttribArray(1);
				glEnableVertexAttribArray(2);
				glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertex), vertices.data(), GL_STATIC_DRAW);
				new_mesh.num_vertices = vertices.size();
				new_prop.parts.push_back(new_mesh);
			}
			props[new_prop_name] = new_prop;
			std::cout << "Loaded prop \"" << new_prop_name << "\". " << new_prop.parts.size() << " parts." << std::endl;
		}
	}
}
