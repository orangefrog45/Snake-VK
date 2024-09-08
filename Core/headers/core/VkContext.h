#pragma once
#include "util/util.h"
#include "core/VkIncl.h"
#include "vk_mem_alloc.h"


namespace SNAKE {
	using FrameInFlightIndex = uint8_t;
	constexpr FrameInFlightIndex MAX_FRAMES_IN_FLIGHT = 2;

	class LogicalDevice {
	public:
		vk::UniqueDevice device;

		void SubmitGraphics(const vk::SubmitInfo& info, std::optional<vk::Fence> fence = std::nullopt) {
			m_graphics_mux.lock();
			SNK_CHECK_VK_RESULT(m_graphics_queue.submit(info, fence.has_value() ? fence.value() : VK_NULL_HANDLE));
			m_graphics_mux.unlock();
		}

		void GraphicsQueueWaitIdle() {
			m_graphics_mux.lock();
			SNK_CHECK_VK_RESULT(m_graphics_queue.waitIdle());
			m_graphics_mux.unlock();
		}

		vk::Result SubmitPresentation(const vk::PresentInfoKHR& info) {
			m_present_mux.lock();
			auto res = m_presentation_queue.presentKHR(info);
			m_present_mux.unlock();
			return res;
		}

	private:
		std::mutex m_graphics_mux;
		std::mutex m_present_mux;

		vk::Queue m_graphics_queue;
		vk::Queue m_presentation_queue;
		friend class VkContext;
		friend class ImGuiLayer;
	};

	struct PhysicalDevice {
		PhysicalDevice() {
			buffer_properties.pNext = &accel_properties;
			accel_properties.pNext = &ray_properties;
		}
		vk::PhysicalDevice device;
		vk::PhysicalDeviceProperties properties;
		vk::PhysicalDeviceDescriptorBufferPropertiesEXT buffer_properties;
		vk::PhysicalDeviceAccelerationStructurePropertiesKHR accel_properties;
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR ray_properties;
	};

	class VkContext {
	public:
		static VkContext& Get() {
			static VkContext s_instance;
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
			return *Get().m_cmd_pools[std::this_thread::get_id()][GetCurrentFIF()];
		}

		static FrameInFlightIndex GetCurrentFIF() {
			return Get().m_current_frame;
		}

		static void CreateCommandPool(const class QueueFamilyIndices& queue_indices);

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
		VkContext(const VkContext& other) = delete;
		VkContext()=default;

		~VkContext() {
			vmaDestroyAllocator(m_allocator);

			for (auto& [thread_id, cmd_pools] : m_cmd_pools) {
				for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
					cmd_pools[i].release();
				}
			}

			m_instance.release();
			m_device.device.release();
		}

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

		std::unordered_map<std::thread::id, std::array<vk::UniqueCommandPool, MAX_FRAMES_IN_FLIGHT>> m_cmd_pools;
		VmaAllocator m_allocator{};

		friend class App;
	};
}
