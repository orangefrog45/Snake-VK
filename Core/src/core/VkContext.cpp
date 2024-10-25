
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

	SNK_ASSERT(m_physical_device.device);

	vk::PhysicalDeviceProperties2 device_properties{};
	device_properties.pNext = &m_physical_device.buffer_properties;
	m_physical_device.device.getProperties2(&device_properties);
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


void VkContext::ICreateLogicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_device_extensions, PFN_vkCreateDevice create_device_func) {
	QueueFamilyIndices indices = FindQueueFamilies(m_physical_device.device, surface);
	float queue_priority = 1.f;

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
	vk::PhysicalDeviceFeatures device_features{};
	device_features.samplerAnisotropy = true;

	vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{};
	descriptor_buffer_features.descriptorBuffer = true;

	vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
	dynamic_rendering_features.dynamicRendering = VK_TRUE;
	dynamic_rendering_features.pNext = &descriptor_buffer_features;

	vk::PhysicalDeviceAccelerationStructureFeaturesKHR accel_features{};
	accel_features.accelerationStructure = true;
	accel_features.pNext = &dynamic_rendering_features;

	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtp_features{};
	rtp_features.rayTracingPipeline = true;
	rtp_features.pNext = &accel_features;

	vk::PhysicalDeviceVulkan12Features features_12{};
	features_12.scalarBlockLayout = true;
	features_12.pNext = &rtp_features;
	features_12.descriptorIndexing = true;
	features_12.bufferDeviceAddress = true;

	vk::PhysicalDeviceFeatures2 f;
	vk::PhysicalDeviceRobustness2FeaturesEXT f2;
	f.pNext = &f2;

	SNK_CORE_CRITICAL(f2.nullDescriptor);

	auto device_create_info = vk::DeviceCreateInfo{}
		.setQueueCreateInfoCount((uint32_t)queue_create_infos.size())
		.setPQueueCreateInfos(queue_create_infos.data())
		.setEnabledExtensionCount((uint32_t)required_device_extensions.size())
		.setPpEnabledExtensionNames(required_device_extensions.data())
		.setPNext(&features_12)
		.setPEnabledFeatures(&device_features);

	if (create_device_func) {
		vk::Device device{};
		auto create_device_res = create_device_func(static_cast<VkPhysicalDevice>(m_physical_device.device), reinterpret_cast<VkDeviceCreateInfo*>(&device_create_info), nullptr, reinterpret_cast<VkDevice*>(&device));
		SNK_CHECK_VK_RESULT(create_device_res);
		m_device.device = std::move(vk::UniqueDevice{ device, m_instance });
	}
	else {
		auto [create_device_res, device] = m_physical_device.device.createDeviceUnique(device_create_info);
		SNK_CHECK_VK_RESULT(create_device_res);
		m_device.device = std::move(device);
	}

	m_device.m_graphics_queue = m_device.device->getQueue(indices.graphics_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
	m_device.m_presentation_queue = m_device.device->getQueue(indices.present_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
}

void VkContext::ICreateInstance(const char* app_name, PFN_vkCreateInstance create_instance_func) {
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

	if (create_instance_func) {
		vk::Instance instance;
		auto res = create_instance_func(reinterpret_cast<VkInstanceCreateInfo*>(&create_info), nullptr, reinterpret_cast<VkInstance*>(&instance));
		SNK_CHECK_VK_RESULT(res);
		m_instance = vk::UniqueInstance{ instance };
	}
	else {
		auto [res, instance] = vk::createInstanceUnique(create_info);
		SNK_CHECK_VK_RESULT(res);
		m_instance = std::move(instance);
	}

	SNK_ASSERT(m_instance);

}
