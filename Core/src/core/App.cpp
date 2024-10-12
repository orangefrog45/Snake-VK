#include "assets/AssetManager.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"
#include "core/JobSystem.h"
#include "core/App.h"
#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "rendering/VkRenderer.h"
#include "rendering/StreamlineWrapper.h"
#include "util/util.h"


#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

using namespace SNAKE;


void App::Init(const char* app_name) {
	JobSystem::Init();
	Logger::Init();

	if (!glfwInit() || !glfwVulkanSupported())
		SNK_BREAK("GLFW failed to initialize");

	window.Init(app_name, 1920, 1080, true);

	PFN_vkGetInstanceProcAddr get_instance_proc_addr_func = nullptr;
	PFN_vkCreateInstance create_instance_func = nullptr;
	PFN_vkCreateDevice create_device_func = nullptr;

	if (StreamlineWrapper::Init()) {
		SNK_CORE_INFO("Streamline is available, loading interposer");
		auto streamline_symbols = StreamlineWrapper::LoadInterposer();
		get_instance_proc_addr_func = streamline_symbols.get_instance_proc_addr;
		create_instance_func = streamline_symbols.create_instance;
		create_device_func = streamline_symbols.create_device;

		VULKAN_HPP_DEFAULT_DISPATCHER.init(get_instance_proc_addr_func);
	}
	else {
		VULKAN_HPP_DEFAULT_DISPATCHER.init();
	}

	VkContext::CreateInstance(app_name, create_instance_func);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(VkContext::GetInstance().get());
	CreateDebugCallback();
	window.CreateSurface();

	std::vector<const char*> required_device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
		VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
		VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,

		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME

	};

	VkContext::PickPhysicalDevice(*window.GetVkContext().surface, required_device_extensions);
	VkContext::CreateLogicalDevice(*window.GetVkContext().surface, required_device_extensions, create_device_func);
	VkContext::InitVMA();
	VkContext::CreateCommandPool(FindQueueFamilies(VkContext::GetPhysicalDevice().device, *window.GetVkContext().surface));

	window.CreateSwapchain();

	AssetManager::Init();
	VkRenderer::Init();

	layers.InitLayers();
}


void App::MainLoop() {
	auto& vk_context = VkContext::Get();

	while (!glfwWindowShouldClose(window.GetGLFWwindow())) {
		EventManagerG::DispatchEvent(FrameSyncFenceEvent{ });

		if (StreamlineWrapper::IsAvailable())
			StreamlineWrapper::AcquireNewFrameToken();

		EventManagerG::DispatchEvent(FrameStartEvent{ });
		layers.OnFrameStart();

		Job* update_job = JobSystem::CreateJob();
		update_job->func = [this]([[maybe_unused]] Job const* job) {
			layers.OnUpdate();  
			EventManagerG::DispatchEvent(EngineUpdateEvent{ });
		};
		JobSystem::Execute(update_job);

		Job* render_job = JobSystem::CreateJob();
		render_job->func = [this]([[maybe_unused]] Job const* job) {
			layers.OnRender();
			EventManagerG::DispatchEvent(EngineRenderEvent{ });
		};
		JobSystem::Execute(render_job);

		JobSystem::WaitAll();
		layers.OnImGuiRender();

		vk_context.m_current_fif = (vk_context.m_current_fif + 1) % MAX_FRAMES_IN_FLIGHT;
		vk_context.m_current_frame_idx++;

		window.OnUpdate();
		glfwPollEvents();
	}

	SNK_CHECK_VK_RESULT(VkContext::GetLogicalDevice().device->waitIdle());
	VkRenderer::Shutdown();
	EventManagerG::DispatchEvent(EngineShutdownEvent{ });
	layers.ShutdownLayers();
	window.Shutdown();
	AssetManager::Shutdown();
	glfwTerminate();
	SNK_CHECK_VK_RESULT(VkContext::GetLogicalDevice().device->waitIdle());

	if (StreamlineWrapper::IsAvailable())
		StreamlineWrapper::Shutdown();

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

	m_messenger = VkContext::GetInstance()->createDebugUtilsMessengerEXTUnique(messenger_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
	SNK_ASSERT(m_messenger);
}


