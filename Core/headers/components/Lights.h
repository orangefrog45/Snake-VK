#pragma once

namespace SNAKE {
	struct DirectionalLight {
		glm::vec3 colour{ 1, 1, 1 };
		glm::vec2 spherical_coords;
		glm::vec3 dir;
	};
}