#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "util/util.h"
#include "core/Window.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"
#include "core/VkIncl.h"
#include "core/DescriptorBuffer.h"
#include "textures/Textures.h"
#include "assets/MeshData.h"
#include "assets/AssetManager.h"
#include "assets/TextureAssets.h"
#include "scene/Scene.h"
#include "scene/CameraSystem.h"
#include "scene/SceneInfoBufferSystem.h"
#include "components/CameraComponent.h"
#include "renderpasses/ShadowPass.h"
#include "components/MeshComponent.h"
#include "renderpasses/ForwardPass.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <chrono>
#include <filesystem>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace SNAKE;

#include "assets/Asset.h"
#include "scene/LightBufferSystem.h"


namespace SNAKE {
	class VulkanApp {
	public:
		void Init(const char* app_name) {
			Window::InitGLFW();
			window.Init(app_name, 1920, 1080, true);
			InitListeners();

			VULKAN_HPP_DEFAULT_DISPATCHER.init();
			VulkanContext::CreateInstance(app_name);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(VulkanContext::GetInstance().get());
			CreateDebugCallback();
			window.CreateSurface();
			VulkanContext::PickPhysicalDevice(*window.m_vk_context.surface, m_required_device_extensions);
			VulkanContext::CreateLogicalDevice(*window.m_vk_context.surface, m_required_device_extensions);
			VulkanContext::InitVMA();
			window.CreateSwapchain();
			// Create command pool which can allocate command buffers
			VulkanContext::CreateCommandPool(&window);


			AssetManager::Init(VulkanContext::GetCommandPool());

			scene.AddSystem<LightBufferSystem>();
			scene.AddSystem<SceneInfoBufferSystem>();
			scene.AddSystem<CameraSystem>();
			scene.CreateEntity()->AddComponent<MeshComponent>();
			auto* p_ent = scene.CreateEntity();
			p_ent->AddComponent<MeshComponent>();
			p_ent->GetComponent<TransformComponent>()->SetPosition({ 0, 0, 4 });
			p_ent->GetComponent<TransformComponent>()->SetScale(0.1, 2, 2);
			p_ent->AddComponent<PointlightComponent>()->colour = glm::vec3(1.f, 0.f, 0.f);
			p_cam_ent = scene.CreateEntity();
			p_cam_ent->AddComponent<CameraComponent>()->MakeActive();
			auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();
			p_transform->SetPosition(0, 0, 8);

			//m_tex = AssetManager::CreateAsset<Texture2DAsset>();
			//m_material = AssetManager::CreateAsset<MaterialAsset>();
			//m_material_2 = AssetManager::CreateAsset<MaterialAsset>();

			//m_tex->p_tex = std::make_unique<Texture2D>();
			//m_tex->p_tex->LoadFromFile("res/textures/oranges_opt.jpg", VulkanContext::GetCommandPool());
			//m_texture_descriptor_buffer.RegisterTexture(*m_tex->p_tex);
			//m_material_descriptor_buffer.RegisterMaterial(m_material);
			//m_material_descriptor_buffer.RegisterMaterial(m_material_2);
			//m_material_2->p_albedo_tex = &*m_tex->p_tex;
			//m_material_2->metallic = 1.f;

			m_shadow_pass.Init();
			m_forward_pass.Init(&window, &m_shadow_pass.m_dir_light_shadow_map);

			CreateSyncObjects();
		}

		void InitListeners() {
			m_framebuffer_resize_listener.callback = [this]([[maybe_unused]] auto event) {
				m_framebuffer_resized = true;
				};

			EventManagerG::RegisterListener<WindowEvent>(m_framebuffer_resize_listener);
		}

		void CreateDebugCallback() {
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


		void CreateSyncObjects() {
			vk::SemaphoreCreateInfo semaphore_info{};
			vk::FenceCreateInfo fence_info{};
			fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // Create in initially signalled state so execution begins immediately

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_image_avail_semaphores.push_back(std::move(VulkanContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value));
				m_render_finished_semaphores.push_back(std::move(VulkanContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value));
				m_in_flight_fences.push_back(std::move(VulkanContext::GetLogicalDevice().device->createFenceUnique(fence_info).value));
			}

			SNK_ASSERT(m_image_avail_semaphores[0] && m_render_finished_semaphores[0] && m_in_flight_fences[0]);
		}

		void MainLoop() {
			while (!glfwWindowShouldClose(window.p_window)) {
				window.OnUpdate();
				glm::vec3 move{ 0,0,0 };
				auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();

				if (window.input.IsMouseDown(1)) {
					auto delta = window.input.GetMouseDelta();
					glm::vec3 forward_rotate_y = glm::angleAxis(-0.01f * delta.x, glm::vec3{ 0, 1, 0 }) * p_transform->forward;
					glm::vec3 forward_rotate_x = glm::angleAxis(-0.01f * delta.y, p_transform->right) * forward_rotate_y;
					p_transform->LookAt(p_transform->GetPosition() + forward_rotate_x);
				}

				if (window.input.IsKeyDown('w'))
					move += p_transform->forward;

				if (window.input.IsKeyDown('s'))
					move -= p_transform->forward;

				if (window.input.IsKeyDown('d'))
					move += p_transform->right;

				if (window.input.IsKeyDown('a'))
					move -= p_transform->right;

				p_transform->SetPosition(p_transform->GetPosition() + move * 0.1f);

				DrawFrame();
				glfwPollEvents();
			}

			glfwTerminate();
		}

		void DrawFrame() {
			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().device->waitForFences(1, &*m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX)
			);

			EventManagerG::DispatchEvent(FrameStartEvent{ m_current_frame });

			uint32_t image_index;
			auto image_result = VulkanContext::GetLogicalDevice().device->acquireNextImageKHR(*window.m_vk_context.swapchain, UINT64_MAX, *m_image_avail_semaphores[m_current_frame], VK_NULL_HANDLE, &image_index);

			// Swap chain suddenly incompatible with surface, likely a window resize
			if (image_result == vk::Result::eErrorOutOfDateKHR) {
				window.RecreateSwapChain();
				return;
			}
			else if (image_result != vk::Result::eSuccess) {
				SNK_BREAK("Failed to acquire swap chain image");
			}

			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().device->resetFences(1, &*m_in_flight_fences[m_current_frame])
			);

			m_forward_pass.RecordCommandBuffer(m_current_frame, window, image_index, scene);
			m_shadow_pass.RecordCommandBuffers(m_current_frame, &scene);

			vk::SubmitInfo depth_submit_info;
			auto shadow_pass_cmd_buf = m_shadow_pass.GetCommandBuffer(m_current_frame);
			depth_submit_info.setCommandBufferCount(1)
				.setPCommandBuffers(&shadow_pass_cmd_buf);

			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().graphics_queue.submit(depth_submit_info)
			);

			vk::SubmitInfo submit_info{};
			auto wait_semaphores = util::array(*m_image_avail_semaphores[m_current_frame]); // Which semaphores to wait on
			auto wait_stages = util::array<vk::PipelineStageFlags>(vk::PipelineStageFlagBits::eColorAttachmentOutput);

			auto forward_cmd_buf = m_forward_pass.GetCommandBuffer(m_current_frame);
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = wait_semaphores.data();
			submit_info.pWaitDstStageMask = wait_stages.data();
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &forward_cmd_buf;

			auto signal_semaphores = util::array(*m_render_finished_semaphores[m_current_frame]);
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = signal_semaphores.data();

			// Queue work to draw triangle, triggers the fence to be active when it starts
			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().graphics_queue.submit(1, &submit_info, *m_in_flight_fences[m_current_frame]) // Fence is signalled once command buffer finishes execution
			);

			auto swapchains = util::array(*window.m_vk_context.swapchain);

			vk::PresentInfoKHR present_info{};
			present_info.waitSemaphoreCount = 1;
			present_info.pWaitSemaphores = signal_semaphores.data();
			present_info.swapchainCount = 1;
			present_info.pSwapchains = swapchains.data();
			present_info.pImageIndices = &image_index;
		
			// Queue presentation work that waits on the render_finished semaphore
			auto present_result = VulkanContext::GetLogicalDevice().presentation_queue.presentKHR(present_info);
			if (present_result == vk::Result::eErrorOutOfDateKHR || m_framebuffer_resized || present_result == vk::Result::eSuboptimalKHR) {
				m_framebuffer_resized = false;
				window.RecreateSwapChain();
				m_forward_pass.OnSwapchainInvalidate({window.GetWidth(), window.GetHeight()});
			}
			else if (present_result != vk::Result::eSuccess) {
				SNK_BREAK("Failed to present image");
			}

			m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
		}


		~VulkanApp() {
			// Wait for any fences before destroying them
			for (auto& fence : m_in_flight_fences) {
				SNK_CHECK_VK_RESULT(
					VulkanContext::GetLogicalDevice().device->waitForFences(*fence, true, std::numeric_limits<uint64_t>::max())
				)
			}
		}

		SNAKE::Window window;
	
	private:
		Scene scene;
		Entity* p_cam_ent = nullptr;

		EventListener m_framebuffer_resize_listener;
	

		ShadowPass m_shadow_pass;
		ForwardPass m_forward_pass;

		std::vector<vk::UniqueSemaphore> m_image_avail_semaphores;
		std::vector<vk::UniqueSemaphore> m_render_finished_semaphores;
		std::vector<vk::UniqueFence> m_in_flight_fences;
		vk::UniqueDebugUtilsMessengerEXT m_messenger;

		bool m_framebuffer_resized = false;

		FrameInFlightIndex m_current_frame = 0;

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

	};
};



int main() {
	//std::filesystem::current_path("../../../../Core");
	SNAKE::Logger::Init();

	SNAKE::VulkanApp app{};
	app.Init("Snake");
	app.MainLoop();

	return 0;
}