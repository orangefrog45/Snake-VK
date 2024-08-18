#include "pch/pch.h"
#include "components/Components.h"
#include "scene/SceneSerializer.h"
#include "util/FileUtil.h"

using namespace SNAKE;

using json = nlohmann::json;

namespace glm {
	void to_json(json& j, const glm::vec3& v) {
		j = json::array({ v.x, v.y, v.z });
	}

	void from_json(const json& j, glm::vec3& v) {
		j.at(0).get_to(v.x);
		j.at(1).get_to(v.y);
		j.at(2).get_to(v.z);
	}
}

#define tget(node, type)node.template get<type>()


json SceneSerializer::SerializeEntity(Entity& ent, Scene& scene) {
	json j;

	{
		j["UUID"] = ent.GetUUID();
		j["Name"] = ent.GetComponent<TagComponent>()->name;
		j["ParentUUID"] = ent.GetParent() == entt::null ? 0 : scene.GetEntity(ent.GetParent())->GetUUID();
	}

	{
		auto* p_transform = ent.GetComponent<TransformComponent>();
		j["Transform"]["P"] = p_transform->GetPosition();
		j["Transform"]["S"] = p_transform->GetScale();
		j["Transform"]["O"] = p_transform->GetOrientation();
		j["Transform"]["Abs"] = false;
	}

	if (auto* p_mesh = ent.GetComponent<StaticMeshComponent>()) {
		j["StaticMesh"]["AssetUUID"] = p_mesh->mesh_asset->uuid();
	}
	
	return j;
}

void SceneSerializer::DeserializeEntity(Entity& ent, json& j) {
	auto* p_tag = ent.GetComponent<TagComponent>();
	p_tag->name = j.at("Name");
	auto& scene = *ent.GetScene();

	uint64_t parent_uuid = tget(j.at("ParentUUID"), uint64_t);
	if (parent_uuid != 0) {
		auto* p_parent = scene.GetEntity(parent_uuid);
		if (!p_parent)
			SNK_CORE_ERROR("Entity [{}, {}] serialized with parent UUID '{}', parent UUID not found", p_tag->name, ent.GetUUID(), p_parent->GetUUID());
		else
			ent.SetParent(*p_parent);
	}

	auto* p_transform = ent.GetComponent<TransformComponent>();
	auto& t_node = j.at("Transform");
	t_node.at("Abs").get_to(p_transform->m_is_absolute);
	t_node.at("P").get_to(p_transform->m_pos);
	t_node.at("S").get_to(p_transform->m_scale);
	t_node.at("O").get_to(p_transform->m_orientation);
	p_transform->RebuildMatrix(TransformComponent::ALL);

	if (j.contains("StaticMesh")) {
		auto& m = j["StaticMesh"];
		ent.AddComponent<StaticMeshComponent>()->mesh_asset = AssetManager::GetAsset<StaticMeshAsset>(tget(m["AssetUUID"], uint64_t));
	}
}


void SceneSerializer::SerializeScene(const std::string& output_filepath, Scene& scene) {
	json j;

	j["Entities"] = json::array();
	for (auto* p_ent : scene.GetEntities()) {
		j["Entities"].push_back(SerializeEntity(*p_ent, scene));
	}

	files::WriteTextFile(output_filepath, j.dump(4));
}

void SceneSerializer::DeserializeScene(const std::string& input_filepath, Scene& scene) {
	std::string content = files::ReadTextFile(input_filepath);
	json j = json::parse(content);

	// First create all entities with UUIDS
	for (auto& ent_node : j.at("Entities")) {
		auto& ent = scene.CreateEntity(tget(ent_node.at("UUID"), uint64_t));
	}

	for (auto& ent_node : j.at("Entities")) {

		DeserializeEntity(*scene.GetEntity(tget(ent_node.at("UUID"), uint64_t)), ent_node);
	}
}