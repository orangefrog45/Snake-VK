#pragma once
#include <vk_mem_alloc.h>
#include "core/VkContext.h"
#include "core/VkIncl.h"
#include "resources/S_VkResource.h"

namespace SNAKE {
	class S_VkBuffer : public S_VkResource {
	public:
		S_VkBuffer() = default;
		S_VkBuffer(const S_VkBuffer& other) = delete;
		S_VkBuffer& operator=(const S_VkBuffer& other) = delete;

		S_VkBuffer(S_VkBuffer&& other) noexcept : 
			allocation(std::move(other.allocation)), 
			buffer(std::move(other.buffer)) {};

		~S_VkBuffer() {
			DestroyBuffer();
		}

		void DestroyBuffer() {
			if (p_data) Unmap();

			if (buffer)
				vmaDestroyBuffer(VkContext::GetAllocator(), buffer, allocation);

			DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::DELETE);
		}

		void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags = 0);
		
		void Resize(size_t new_size);

		void RefreshDescriptorGetInfo(DescriptorGetInfo& info) const override;

		void MemoryBarrier(vk::AccessFlagBits src_access_mask, vk::AccessFlagBits dst_access_mask, vk::PipelineStageFlagBits src_stage, 
			vk::PipelineStageFlagBits dst_stage, vk::CommandBuffer buf = {});

		DescriptorGetInfo CreateDescriptorGetInfo() const;

		vk::DeviceAddress GetDeviceAddress() const {
			vk::BufferDeviceAddressInfo buffer_addr_info{};
			buffer_addr_info.buffer = buffer;
			return VkContext::GetLogicalDevice().device->getBufferAddress(buffer_addr_info);
		}

		inline void* Map() {
			if (p_data) return p_data;

			SNK_CHECK_VK_RESULT(vmaMapMemory(VkContext::GetAllocator(), allocation, &p_data));
			return p_data;
		}

		inline void Unmap() {
			vmaUnmapMemory(VkContext::GetAllocator(), allocation);
			p_data = nullptr;
		}

		VmaAllocation allocation = nullptr;
		VmaAllocationInfo alloc_info{};
		vk::Buffer buffer = VK_NULL_HANDLE;

	private:
		vk::BufferUsageFlags m_usage = vk::BufferUsageFlagBits(0);
		VmaAllocationCreateFlags m_alloc_create_flags = 0;
		// Mapped ptr, nullptr if not mapped
		void* p_data = nullptr;
	};
}
