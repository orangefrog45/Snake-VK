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

	void to_json(json& j, const glm::vec2& v) {
		j = json::array({ v.x, v.y });
	}

	void from_json(const json& j, glm::vec2& v) {
		j.at(0).get_to(v.x);
		j.at(1).get_to(v.y);
	}
}

namespace SNAKE {
	void to_json(json& j, const LightAttenuation& atten) {
		j = json::array({ atten.constant, atten.linear, atten.exp });
	}

	void from_json(const json& j, LightAttenuation& atten) {
		j.at(0).get_to(atten.constant);
		j.at(1).get_to(atten.linear);
		j.at(2).get_to(atten.exp);
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
		std::vector<uint64_t> material_uuids;
		for (size_t i = 0; i < p_mesh->materials.size(); i++) {
			material_uuids.push_back(p_mesh->materials[i]->uuid());
		}
		j["StaticMesh"]["Materials"] = material_uuids;
	}

	if (auto* p_pl = ent.GetComponent<PointlightComponent>()) {
		j["Pointlight"]["Colour"] = p_pl->colour;
		j["Pointlight"]["Attenuation"] = p_pl->attenuation;
	}

	if (auto* p_sl = ent.GetComponent<SpotlightComponent>()) {
		j["Spotlight"]["Colour"] = p_sl->colour;
		j["Spotlight"]["Attenuation"] = p_sl->attenuation;
		j["Spotlight"]["Aperture"] = p_sl->aperture;
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
			SNK_CORE_ERROR("Entity [{}, {}] serialized with parent UUID '{}', parent UUID not found", p_tag->name, ent.GetUUID(), parent_uuid);
		else
			ent.SetParent(*p_parent);
	}

	{
		auto* p_transform = ent.GetComponent<TransformComponent>();
		auto& t = j.at("Transform");
		t.at("Abs").get_to(p_transform->m_is_absolute);
		t.at("P").get_to(p_transform->m_pos);
		t.at("S").get_to(p_transform->m_scale);
		t.at("O").get_to(p_transform->m_orientation);
		p_transform->RebuildMatrix(TransformComponent::ALL);
	}

	if (j.contains("Pointlight")) {
		auto* p_comp = ent.AddComponent<PointlightComponent>();
		auto& p = j.at("Pointlight");
		p.at("Colour").get_to(p_comp->colour);
		p.at("Attenuation").get_to(p_comp->attenuation);
	}

	if (j.contains("Spotlight")) {
		auto* p_comp = ent.AddComponent<SpotlightComponent>();
		auto& s = j.at("Spotlight");
		s.at("Colour").get_to(p_comp->colour);
		s.at("Attenuation").get_to(p_comp->attenuation);
		s.at("Aperture").get_to(p_comp->aperture);
	}
	

	if (j.contains("StaticMesh")) {
		auto& m = j["StaticMesh"];
		auto* p_mesh = ent.AddComponent<StaticMeshComponent>();
		p_mesh->SetMeshAsset(AssetManager::GetAsset<StaticMeshAsset>(tget(m["AssetUUID"], uint64_t)));
		std::vector<uint64_t> material_uuids;
		m.at("Materials").get_to<std::vector<uint64_t>>(material_uuids);
		for (size_t i = 0; i < material_uuids.size(); i++) {
			auto uuid = material_uuids[i];
			auto mat = AssetManager::GetAsset<MaterialAsset>(uuid);
			if (!mat) {
				SNK_CORE_ERROR("Tried deserializing material {} for mesh component but UUID not found, replacing with default", uuid);
				mat = AssetManager::GetAsset<MaterialAsset>(AssetManager::CoreAssetIDs::MATERIAL);
			}

			p_mesh->materials[i] = mat;
		}
	}
	
}


void SceneSerializer::SerializeScene(const std::string& output_filepath, Scene& scene) {
	json j;

	j["SceneUUID"] = scene.uuid();
	j["Entities"] = json::array();
	for (auto* p_ent : scene.GetEntities()) {
		j["Entities"].push_back(SerializeEntity(*p_ent, scene));
	}

	{
		j["DirLight"]["Colour"] = scene.directional_light.colour;
		j["DirLight"]["SphericalCoords"] = scene.directional_light.spherical_coords;
	}

	files::WriteTextFile(output_filepath, j.dump(4));
}

void SceneSerializer::DeserializeScene(const std::string& input_filepath, Scene& scene) {
	std::string content = files::ReadTextFile(input_filepath);
	json j = json::parse(content);

	scene.uuid = UUID<uint64_t>(tget(j.at("SceneUUID"), uint64_t));

	// First create all entities with UUIDS
	for (auto& ent_node : j.at("Entities")) {
		scene.CreateEntity(tget(ent_node.at("UUID"), uint64_t));
	}

	for (auto& ent_node : j.at("Entities")) {
		DeserializeEntity(*scene.GetEntity(tget(ent_node.at("UUID"), uint64_t)), ent_node);
	}

	j.at("DirLight").at("Colour").get_to(scene.directional_light.colour);
	j.at("DirLight").at("SphericalCoords").get_to(scene.directional_light.spherical_coords);
}