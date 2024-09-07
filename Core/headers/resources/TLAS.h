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

		DescriptorGetInfo CreateDescriptorGetInfo();
	private:
		vk::UniqueAccelerationStructureKHR m_as;

		vk::DeviceAddress m_tlas_handle;
		S_VkBuffer instance_buffer;
		S_VkBuffer m_as_buffer;
	};
}