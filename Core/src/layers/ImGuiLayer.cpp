#include "pch/pch.h"
#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"
#include "layers/ImGuiLayer.h"
#include "util/util.h"

using namespace SNAKE;

void ImGuiLayer::OnInit() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplGlfw_InitForVulkan(mp_window->GetGLFWwindow(), true);

	vk::Format swapchain_format = mp_window->GetVkContext().swapchain_format;
	vk::PipelineRenderingCreateInfo pipeline_info{};
	pipeline_info.pColorAttachmentFormats = &swapchain_format;
	pipeline_info.colorAttachmentCount = 1;
	QueueFamilyIndices indices = FindQueueFamilies(VulkanContext::GetPhysicalDevice().device, *mp_window->GetVkContext().surface);

	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	auto [err, val] = VulkanContext::GetLogicalDevice().device->createDescriptorPoolUnique(pool_info);
	SNK_CHECK_VK_RESULT(err);

	m_imgui_descriptor_pool = std::move(val);

	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.UseDynamicRendering = true;
	init_info.PipelineRenderingCreateInfo = static_cast<VkPipelineRenderingCreateInfo>(pipeline_info);
	init_info.Instance = *VulkanContext::GetInstance();
	init_info.PhysicalDevice = VulkanContext::GetPhysicalDevice().device;
	init_info.Device = *VulkanContext::GetLogicalDevice().device;
	init_info.QueueFamily = indices.graphics_family.value();
	init_info.Queue = VulkanContext::GetLogicalDevice().graphics_queue;
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = *m_imgui_descriptor_pool;
	init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
	init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);
	ImGui_ImplVulkan_CreateFontsTexture();
	ImGui::StyleColorsDark();

};


void ImGuiLayer::OnImGuiStartRender() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}


void ImGuiLayer::OnImGuiEndRender() {
	ImGui::Render();
}

void ImGuiLayer::OnShutdown() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}