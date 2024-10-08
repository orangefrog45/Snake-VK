#include "pch/pch.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"
#include "core/Window.h"
#include "core/JobSystem.h"
#include "util/util.h"

using namespace SNAKE;

void VkContext::IPickPhysicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec) {
	uint32_t device_count = 0;
	auto res = m_instance->enumeratePhysicalDevices(&device_count, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);
	SNK_CHECK_VK_RESULT(res);

	if (device_count == 0)
		SNK_BREAK("No physical devices found for Vulkan, exiting");

	std::vector<vk::PhysicalDevice> devices(device_count);
	auto res2 = m_instance->enumeratePhysicalDevices(&device_count, devices.data(), VULKAN_HPP_DEFAULT_DISPATCHER);
	SNK_CHECK_VK_RESULT(res2);

	for (const auto& device : devices) {
		vk::PhysicalDeviceProperties device_properties;
		if (IsDeviceSuitable(device, surface, required_extensions_vec)) {
			m_physical_device.device = device;
			device.getProperties(&device_properties, VULKAN_HPP_DEFAULT_DISPATCHER);
			m_physical_device.properties = device_properties;
			SNK_CORE_INFO("Suitable physical device found '{0}'", device_properties.deviceName.data());
		}
	}

	vk::PhysicalDeviceProperties2 device_properties{};
	device_properties.pNext = &m_physical_device.buffer_properties;
	m_physical_device.device.getProperties2(&device_properties);

	SNK_ASSERT(m_physical_device.device);
}


void VkContext::CreateCommandPool(const QueueFamilyIndices& queue_indices) {
	auto num_threads = std::thread::hardware_concurrency();
	std::vector<std::thread::id> thread_ids = JobSystem::GetThreadIDs();

	for (unsigned i = 0; i < num_threads; i++) {
		for (FrameInFlightIndex fif = 0; fif < MAX_FRAMES_IN_FLIGHT; fif++) {
			vk::CommandPoolCreateInfo pool_info{};
			pool_info.queueFamilyIndex = queue_indices.graphics_family.value();
			pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

			auto [res, value] = VkContext::GetLogicalDevice().device->createCommandPoolUnique(pool_info);
			Get().m_cmd_pools[thread_ids[i]][fif] = std::move(value);
			SNK_CHECK_VK_RESULT(res);
		}
	}
}


bool VkContext::IsDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec) {
	vk::PhysicalDeviceProperties properties;
	device.getProperties(&properties, VULKAN_HPP_DEFAULT_DISPATCHER);

	if (!CheckDeviceExtensionSupport(device, required_extensions_vec))
		return false;

	SwapChainSupportDetails swapchain_support = QuerySwapChainSupport(device, surface);
	bool swapchain_valid = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();

	QueueFamilyIndices qf_indices = FindQueueFamilies(device, surface);

	vk::PhysicalDeviceFeatures device_features = device.getFeatures();
	return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && qf_indices.IsComplete() && swapchain_valid && device_features.samplerAnisotropy;
}


void VkContext::ICreateLogicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_device_extensions) {
	QueueFamilyIndices indices = FindQueueFamilies(m_physical_device.device, surface);
	float queue_priority = 1.f;


	vk::PhysicalDeviceFeatures device_features{};
	device_features.samplerAnisotropy = true;

	std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
	std::set<uint32_t> unique_queue_families = { indices.graphics_family.value(), indices.present_family.value() };

	for (uint32_t queue_family : unique_queue_families) {
		vk::DeviceQueueCreateInfo queue_create_info{
		vk::DeviceQueueCreateFlags(0),
		queue_family,
		1,
		&queue_priority,
		nullptr
		};

		queue_create_infos.push_back(queue_create_info);
	}

	// Features
	vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{};
	descriptor_buffer_features.descriptorBuffer = true;

	vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
	buffer_device_address_features.bufferDeviceAddress = true;
	buffer_device_address_features.pNext = &descriptor_buffer_features;

	vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
	dynamic_rendering_features.dynamicRendering = VK_TRUE;
	dynamic_rendering_features.pNext = &buffer_device_address_features;

	vk::PhysicalDeviceAccelerationStructureFeaturesKHR accel_features{};
	accel_features.accelerationStructure = true;
	accel_features.pNext = &dynamic_rendering_features;

	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtp_features{};
	rtp_features.rayTracingPipeline = true;
	rtp_features.pNext = &accel_features;

	vk::PhysicalDeviceScalarBlockLayoutFeatures scalar_layout_features{};
	scalar_layout_features.scalarBlockLayout = true;
	scalar_layout_features.pNext = &rtp_features;

	auto device_create_info = vk::DeviceCreateInfo{}
		.setQueueCreateInfoCount((uint32_t)queue_create_infos.size())
		.setPQueueCreateInfos(queue_create_infos.data())
		.setEnabledExtensionCount((uint32_t)required_device_extensions.size())
		.setPpEnabledExtensionNames(required_device_extensions.data())
		.setPNext(&scalar_layout_features)
		.setPEnabledFeatures(&device_features);


	m_device.device = m_physical_device.device.createDeviceUnique(device_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
	SNK_ASSERT(m_device.device);

	m_device.m_graphics_queue = m_device.device->getQueue(indices.graphics_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
	m_device.m_presentation_queue = m_device.device->getQueue(indices.present_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
}

void VkContext::ICreateInstance(const char* app_name) {
	std::vector<const char*> layers = {
		"VK_LAYER_KHRONOS_validation"
	};

	std::vector<const char*> extensions = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined (_WIN32)
		"VK_KHR_win32_surface",
#endif
#if defined (__APPLE__)
		"VK_MVK_macos_surface"
#endif
#if defined (__linux__)
		"VK_KHR_xcb_surface"
#endif
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

	vk::ApplicationInfo app_info{
		app_name,
		VK_MAKE_API_VERSION(0, 1, 0, 0),
		"Snake",
		VK_MAKE_API_VERSION(0, 1, 0, 0),
		VK_API_VERSION_1_3
	};

	vk::InstanceCreateInfo create_info{
		vk::Flags<vk::InstanceCreateFlagBits>(0),
		&app_info,
		{(uint32_t)layers.size(), layers.data()},
		{(uint32_t)extensions.size(), extensions.data()}
	};

	m_instance = vk::createInstanceUnique(create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
	SNK_ASSERT(m_instance);

}
