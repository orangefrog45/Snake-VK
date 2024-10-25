#pragma once
#include "S_VkBuffer.h"

namespace SNAKE {
	class TLAS : public S_VkResource {
	public:
		void BuildFromInstances(const std::vector<vk::AccelerationStructureInstanceKHR>& instances);

		void Destroy();

		vk::AccelerationStructureKHR GetAS() {
			return *m_as;
		}

		void RefreshDescriptorGetInfo(DescriptorGetInfo& info) const override;

		DescriptorGetInfo CreateDescriptorGetInfo() const;
	private:
		vk::UniqueAccelerationStructureKHR m_as;

		vk::DeviceAddress m_tlas_handle;
		S_VkBuffer instance_buffer;
		S_VkBuffer m_as_buffer;
	};

	class BLAS { // Not considered a resource as BLAS' are only used internally by TLAS
	public:
		void GenerateFromMeshData(struct MeshData& mesh_data, uint32_t submesh_index);

		vk::AccelerationStructureInstanceKHR GenerateInstance(class TransformComponent& comp);
	private:
		uint32_t m_submesh_index = std::numeric_limits<uint32_t>::max();

		S_VkBuffer m_blas_buf;
		vk::UniqueAccelerationStructureKHR mp_blas;
		vk::DeviceAddress m_blas_handle = 0;
	};
}