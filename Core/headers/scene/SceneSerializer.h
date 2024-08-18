#pragma once
#include "Scene.h"
#include "nlohmann/json.hpp"

namespace SNAKE {
	class SceneSerializer {
	public:
		static void SerializeScene(const std::string& output_filepath, Scene& scene);

		static void DeserializeScene(const std::string& input_filepath, Scene& scene);

		static nlohmann::json SerializeEntity(Entity& ent, Scene& scene);

		static void DeserializeEntity(Entity& ent, nlohmann::json& j);

	};
}