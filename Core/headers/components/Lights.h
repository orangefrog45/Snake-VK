#pragma once
#include "Component.h"
namespace SNAKE {
	class Entity;

	struct LightAttenuation {
		float constant = 0.5f;
		float linear = 0.25f;
		float exp = 0.05f;
	};

	struct DirectionalLight {
		glm::vec3 colour{ 1, 1, 1 };
		glm::vec2 spherical_coords{ 0, 0 };
		glm::vec3 dir{ 1, 0, 0 };
	};

	struct SpotlightComponent : public Component {
		SpotlightComponent(Entity* p_ent) : Component(p_ent) { };

		glm::vec3 colour{ 1, 0, 0 };
		float aperture = 0.15f;
		LightAttenuation attenuation{};
	};

	struct PointlightComponent : public Component {
		PointlightComponent(Entity* p_ent) : Component(p_ent) { };
		glm::vec3 colour{ 1, 1, 1 };
		LightAttenuation attenuation{};
	};



}