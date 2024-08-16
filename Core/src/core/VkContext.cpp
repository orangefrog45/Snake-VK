#include "pch/pch.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"
#include "util/util.h"
#include "core/Window.h"

using namespace SNAKE;

void VulkanContext::IPickPhysicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec) {
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

	SNK_ASSERT(m_physical_device.device, "Physical device created");
}


void VulkanContext::CreateCommandPool(Window* p_window) {
	QueueFamilyIndices qf_indices = FindQueueFamilies(VulkanContext::GetPhysicalDevice().device, *p_window->GetVkContext().surface);

	vk::CommandPoolCreateInfo pool_info{};
	pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // allow command buffers under this pool to be rerecorded individually instead of having to be reset together
	pool_info.queueFamilyIndex = qf_indices.graphics_family.value();

	Get().m_cmd_pool = VulkanContext::GetLogicalDevice().device->createCommandPoolUnique(pool_info).value;
	SNK_ASSERT(Get().m_cmd_pool, "Command pool created");
}


bool VulkanContext::IsDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*>& required_extensions_vec) {
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


void VulkanContext::ICreateLogicalDevice(vk::SurfaceKHR surface, const std::vector<const char*>& required_device_extensions) {
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

	vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{};
	descriptor_buffer_features.descriptorBuffer = true;

	vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
	buffer_device_address_features.bufferDeviceAddress = true;
	buffer_device_address_features.pNext = &descriptor_buffer_features;

	vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
	dynamic_rendering_features.dynamicRendering = VK_TRUE;
	dynamic_rendering_features.pNext = &buffer_device_address_features;

	auto device_create_info = vk::DeviceCreateInfo{}
		.setQueueCreateInfoCount((uint32_t)queue_create_infos.size())
		.setPQueueCreateInfos(queue_create_infos.data())
		.setEnabledExtensionCount((uint32_t)required_device_extensions.size())
		.setPpEnabledExtensionNames(required_device_extensions.data())
		.setPNext(&dynamic_rendering_features)
		.setPEnabledFeatures(&device_features);


	m_device.device = m_physical_device.device.createDeviceUnique(device_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
	SNK_ASSERT(m_device.device, "Logical device creation");

	m_device.graphics_queue = m_device.device->getQueue(indices.graphics_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
	m_device.presentation_queue = m_device.device->getQueue(indices.present_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
}

void VulkanContext::ICreateInstance(const char* app_name) {
	std::vector<const char*> layers = {
		//"VK_LAYER_KHRONOS_validation"
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
