#pragma once
#include "util/util.h"
#include "vk_mem_alloc.h"
#include <core/VkIncl.h>


namespace SNAKE {
	struct LogicalDevice {
		vk::UniqueDevice device;

		vk::Queue graphics_queue;
		vk::Queue presentation_queue;
	};

	struct PhysicalDevice {
		vk::PhysicalDevice device;
		vk::PhysicalDeviceProperties properties;
	};

	class VulkanContext {
	public:
		static VulkanContext& Get() {
			static VulkanContext s_instance;
			return s_instance;
		}

		static void CreateInstance(const char* app_name) {
			SNK_ASSERT(!Get().m_instance, "Instance not already created");
			Get().ICreateInstance(app_name);
		}

		static void PickPhysicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec) {
			SNK_ASSERT(!Get().m_physical_device.device, "Physical device not already created");
			Get().IPickPhysicalDevice(surface, required_extensions_vec);
		}

		static void CreateLogicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_device_extensions) {
			SNK_ASSERT(!Get().m_device.device, "Logical device not already created");
			Get().ICreateLogicalDevice(surface, required_device_extensions);
		}

		static void InitVMA() {
			Get().I_InitVMA();
		}

		static auto& GetAllocator() {
			return Get().m_allocator;
		}

		static auto& GetInstance() {
			return Get().m_instance;
		}

		static auto& GetPhysicalDevice() {
			return Get().m_physical_device;
		}

		static auto& GetLogicalDevice() {
			return Get().m_device;
		}

	private:
		VulkanContext(const VulkanContext& other) = delete;
		VulkanContext()=default;

		void I_InitVMA() {
			VmaAllocatorCreateInfo alloc_info{};
			alloc_info.device = *m_device.device;
			alloc_info.physicalDevice = m_physical_device.device;
			alloc_info.instance = *m_instance;
			alloc_info.flags = VmaAllocatorCreateFlagBits::VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
			SNK_CHECK_VK_RESULT(vmaCreateAllocator(&alloc_info, &m_allocator));
		}

		void ICreateInstance(const char* app_name);

		void IPickPhysicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec);

		static bool IsDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec);

		void ICreateLogicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_device_extensions);

		vk::UniqueInstance m_instance;

		PhysicalDevice m_physical_device;
		LogicalDevice m_device;

		VmaAllocator m_allocator;

		friend class VulkanApp;
	};
}
