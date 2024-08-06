#pragma once

namespace SNAKE {
	class ExtraMath
	{
	public:

		struct Plane {
			Plane() = default;
			Plane(glm::vec3 t_normal, glm::vec3 p1) : normal(glm::normalize(t_normal)), distance(glm::dot(normal, p1)) {};
			float GetSignedDistanceToPlane(const glm::vec3& point) const { return glm::dot(normal, point) - distance; }
			glm::vec3 normal = { 0.0f, 1.0f, 0.0f };
			float distance = 0.0f;
		};

		struct Frustum {
			Plane top_plane;
			Plane bottom_plane;
			Plane right_plane;
			Plane left_plane;
			Plane far_plane;
			Plane near_plane;
		};


		// Returns a quaternion that rotates v onto w by factor "interp" (0-1)
		static glm::quat MapVectorTransform(const glm::vec3& v, const glm::vec3& w, float interp = 1.f);

		static glm::mat4 CalculateLightSpaceMatrix(const glm::mat4& proj, const glm::mat4& view, glm::vec3 light_dir, float z_mult, float shadow_map_size);

		static glm::vec3 AngleAxisRotateAroundPoint(glm::vec3 rotation_center, glm::vec3 point_to_rotate, glm::vec3 axis, float angle);

		static std::array<glm::vec4, 8> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
		static glm::mat3 Rotate3D(float rotX, float rotY, float rotZ);
		static glm::mat3 Scale3D(float scaleX, float scaleY, float scaleZ);
		static glm::mat4x4 Translation3D(float tranX, float tranY, float tranZ);
		static glm::mat3 Scale2D(float x, float y);
		static glm::mat3 Rotate2D(float rot);
		static glm::mat3 Translation2D(float x, float y);

		static glm::vec3 ScreenCoordsToRayDir(glm::mat4 proj_matrix, glm::vec2 coords, glm::vec3 cam_pos, glm::vec3 cam_forward, glm::vec3 cam_up, unsigned int window_width, unsigned int window_height);
	};
}