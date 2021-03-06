#pragma once

#include <mirrage/renderer/animation.hpp>

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

struct aiScene;
struct aiNode;
struct aiBone;

namespace mirrage {
	struct Mesh_converted_config;

	struct Bone_data {
		const aiNode* assimp_node;
		const aiBone* assimp_bone;

		std::string                    name;
		int                            idx = -1;
		std::string                    parent_name;
		int                            parent_idx = -1;
		glm::mat3x4                    offset{1};
		renderer::Local_bone_transform local_node_transform{};

		Bone_data() = default;
		Bone_data(const aiNode&                         node,
		          const renderer::Local_bone_transform& loca_node_transform,
		          int                                   idx,
		          int                                   parent_idx);
	};
	struct Skeleton_data {
		std::vector<Bone_data>               bones;
		std::unordered_map<std::string, int> bones_by_name;
	};

	extern auto parse_skeleton(const std::string&           model_name,
	                           const std::string&           output,
	                           const aiScene&               scene,
	                           const Mesh_converted_config& cfg) -> Skeleton_data;
} // namespace mirrage
