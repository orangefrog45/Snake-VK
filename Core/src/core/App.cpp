#include "assets/AssetManager.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"
#include "core/JobSystem.h"
#include "core/App.h"
#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "rendering/VkRenderer.h"
#include "util/util.h"

#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

using namespace SNAKE;


void App::Init(const char* app_name) {
	JobSystem::Init();

	Logger::Init();

	Window::InitGLFW();
	window.Init(app_name, 1920, 1080, true);

	VULKAN_HPP_DEFAULT_DISPATCHER.init();
	VulkanContext::CreateInstance(app_name);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(VulkanContext::GetInstance().get());
	CreateDebugCallback();
	window.CreateSurface();

	std::vector<const char*> m_required_device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
		VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	};

	VulkanContext::PickPhysicalDevice(*window.GetVkContext().surface, m_required_device_extensions);
	VulkanContext::CreateLogicalDevice(*window.GetVkContext().surface, m_required_device_extensions);
	VulkanContext::InitVMA();
	window.CreateSwapchain();
	VulkanContext::CreateCommandPool(&window);

	AssetManager::Init(VulkanContext::GetCommandPool());
	VkRenderer::Init();

	layers.InitLayers();
}


void App::MainLoop() {
	while (!glfwWindowShouldClose(window.GetGLFWwindow())) {
		glfwPollEvents();
		EventManagerG::DispatchEvent(FrameSyncFenceEvent{ });

		EventManagerG::DispatchEvent(FrameStartEvent{ });

		window.OnUpdate();
		Job* update_job = JobSystem::CreateJob();
		update_job->func = [this]([[maybe_unused]] Job const* job) {layers.OnUpdate(); };
		JobSystem::Execute(update_job);
		EventManagerG::DispatchEvent(EngineUpdateEvent{ });

		Job* render_job = JobSystem::CreateJob();
		render_job->func = [this]([[maybe_unused]] Job const* job) {layers.OnRender(); };
		JobSystem::Execute(render_job);
		EventManagerG::DispatchEvent(EngineRenderEvent{ });

		JobSystem::Wait();
		layers.OnImGuiRender();

		VulkanContext::Get().m_current_frame = (VulkanContext::Get().m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	VulkanContext::GetLogicalDevice().device->waitIdle();
	VkRenderer::Shutdown();
	EventManagerG::DispatchEvent(EngineShutdownEvent{ });
	layers.ShutdownLayers();
	window.Shutdown();
	AssetManager::Shutdown();
	glfwTerminate();
	VulkanContext::GetLogicalDevice().device->waitIdle();

	JobSystem::Shutdown();
}

App::~App() {
}

void App::CreateDebugCallback() {
	vk::DebugUtilsMessageSeverityFlagsEXT severity_flags =
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;

	vk::DebugUtilsMessageTypeFlagsEXT type_flags =
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

	vk::DebugUtilsMessengerCreateInfoEXT messenger_create_info{
		vk::DebugUtilsMessengerCreateFlagBitsEXT(0),
		severity_flags,
		type_flags,
		&DebugCallback,
		nullptr
	};

	m_messenger = VulkanContext::GetInstance()->createDebugUtilsMessengerEXTUnique(messenger_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
	SNK_ASSERT(m_messenger);
}


