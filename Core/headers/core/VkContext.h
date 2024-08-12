#pragma once
#include "util/util.h"
#include "vk_mem_alloc.h"
#include <core/VkIncl.h>


namespace SNAKE {
	using FrameInFlightIndex = uint8_t;

	struct LogicalDevice {
		vk::UniqueDevice device;

		vk::Queue graphics_queue;
		vk::Queue presentation_queue;
	};

	struct PhysicalDevice {
		vk::PhysicalDevice device;
		vk::PhysicalDeviceProperties properties;
		vk::PhysicalDeviceDescriptorBufferPropertiesEXT buffer_properties;
	};

	class VulkanContext {
	public:
		static VulkanContext& Get() {
			static VulkanContext s_instance;
			return s_instance;
		}

		static void CreateInstance(const char* app_name) {
			SNK_ASSERT(!Get().m_instance);
			Get().ICreateInstance(app_name);
		}

		static void PickPhysicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec) {
			SNK_ASSERT(!Get().m_physical_device.device);
			Get().IPickPhysicalDevice(surface, required_extensions_vec);
		}

		static void CreateLogicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_device_extensions) {
			SNK_ASSERT(!Get().m_device.device);
			Get().ICreateLogicalDevice(surface, required_device_extensions);
		}

		static vk::CommandPool GetCommandPool() {
			return *Get().m_cmd_pool;
		}

		static FrameInFlightIndex GetCurrentFIF() {
			return Get().m_current_frame;
		}

		static void CreateCommandPool(class Window* p_window);

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

		FrameInFlightIndex m_current_frame = 0;

		vk::UniqueInstance m_instance;

		PhysicalDevice m_physical_device;
		LogicalDevice m_device;

		vk::UniqueCommandPool m_cmd_pool;

		VmaAllocator m_allocator{};

		friend class App;
	};
}
