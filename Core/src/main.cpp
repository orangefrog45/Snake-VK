#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "util/FileUtil.h"
#include "util/util.h"
#include "core/Window.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"
#include "core/VkIncl.h"
#include "core/S_VkBuffer.h"
#include "core/DescriptorBuffer.h"
#include "core/Pipelines.h"
#include "core/VkCommands.h"
#include "textures/Textures.h"
#include "assets/MeshData.h"
#include "assets/AssetManager.h"
#include "assets/TextureAssets.h"
#include "scene/Scene.h"

#include <stb_image.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <filesystem>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace SNAKE;

enum class DescriptorSetIndices {
	CAMERA_MATRIX_UBO = 0,
	GLOBAL_TEX_MAT,
	LIGHTS
};

static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
	[[maybe_unused]] void* p_user_data
) {
	std::string type_str;
	switch (type) {
	case (int)vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral:
		type_str = "general";
		break;
	case (int)vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance:
		type_str = "performance";
		break;
	case (int)vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation:
		type_str = "validation";
		break;
	}

	switch (severity) {
	case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
		SNK_CORE_ERROR("Vulkan error '{0}': {1}", type_str, p_callback_data->pMessage);
		SNK_DEBUG_BREAK("Breaking for vulkan error");
		break;
	case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
		SNK_CORE_INFO("Vulkan info '{0}': {1}", type_str, p_callback_data->pMessage);
		break;
	case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
		SNK_CORE_WARN("Vulkan warning '{0}': {1}", type_str, p_callback_data->pMessage);
		break;
	case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
		SNK_CORE_TRACE("Vulkan verbose info '{0}': {1}", type_str, p_callback_data->pMessage);
		break;
	default:
		break;
	}

	return VK_FALSE;
};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphics_family;
	std::optional<uint32_t> present_family;

	bool IsComplete() {
		return graphics_family.has_value() && present_family.has_value();
	}
};

struct SwapChainSupportDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> present_modes;
};

struct Vertex {
	// Struct which describes how to move through the data provided
	[[nodiscard]] constexpr inline static std::array<vk::VertexInputBindingDescription, 3> GetBindingDescription() {
		std::array<vk::VertexInputBindingDescription, 3> descs{};

		descs[0].binding = 0;
		descs[0].stride = sizeof(glm::vec3);
		descs[0].inputRate = vk::VertexInputRate::eVertex;

		descs[1].binding = 1;
		descs[1].stride = sizeof(glm::vec3);
		descs[1].inputRate = vk::VertexInputRate::eVertex;

		descs[2].binding = 2;
		descs[2].stride = sizeof(glm::vec2);
		descs[2].inputRate = vk::VertexInputRate::eVertex;

		return descs;
	}

	// Struct which describes vertex attribute access in shaders
	[[nodiscard]] constexpr inline static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions() {
		std::array<vk::VertexInputAttributeDescription, 3> descs{};
		// Positions
		descs[0].binding = 0;
		descs[0].location = 0;
		descs[0].format = vk::Format::eR32G32B32Sfloat;
		descs[0].offset = 0;

		// Normals
		descs[1].binding = 1;
		descs[1].location = 1;
		descs[1].format = vk::Format::eR32G32B32Sfloat;
		descs[1].offset = 0;

		// Texture coordinates
		descs[2].binding = 2;
		descs[2].location = 2;
		descs[2].format = vk::Format::eR32G32Sfloat;
		descs[2].offset = 0;

		return descs;
	}
};

struct UBO {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

#include "assets/Asset.h"

struct RenderableComponent : Component {
	RenderableComponent(Entity* p_entity) : Component(p_entity) {};
};

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

			// Setup code
			CreateDebugCallback();
			window.CreateSurface();
			VulkanContext::PickPhysicalDevice(*window.m_vk_context.surface, m_required_device_extensions);
			VulkanContext::CreateLogicalDevice(*window.m_vk_context.surface, m_required_device_extensions);
			VulkanContext::InitVMA();
			window.CreateSwapchain();


			// Create command pool which can allocate command buffers
			CreateCommandPool();

			CreateDepthResources();

			scene.CreateEntity()->AddComponent<RenderableComponent>();
			auto* p_ent = scene.CreateEntity();
			p_ent->AddComponent<RenderableComponent>();
			p_ent->GetComponent<TransformComponent>()->SetPosition({ 4, 0, 0 });
			p_ent->GetComponent<TransformComponent>()->SetScale(1, 2, 2);

			p_cam_ent = scene.CreateEntity();
			auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();
			p_transform->SetPosition(0, 0, 8);

			AssetManager::Init();

			m_tex = AssetManager::CreateAsset<Texture2DAsset>();
			m_material = AssetManager::CreateAsset<MaterialAsset>();
			m_material_2 = AssetManager::CreateAsset<MaterialAsset>();

			m_tex->p_tex = std::make_unique<Texture2D>();
			m_tex->p_tex->LoadFromFile("res/textures/oranges_opt.jpg", *m_cmd_pool);
			//m_texture_descriptor_buffer.RegisterTexture(*m_tex->p_tex);
			//m_material_descriptor_buffer.RegisterMaterial(m_material);
			//m_material_descriptor_buffer.RegisterMaterial(m_material_2);
			m_material_2->p_albedo_tex = &*m_tex->p_tex;
			m_material_2->metallic = 1.f;

			CreateUniformBuffers();

			SetupShadowMapping();

			CreateGraphicsPipeline();

			CreateDescriptorBuffers();

			m_mesh = AssetManager::CreateAsset<StaticMeshAsset>();
			m_mesh->data = std::make_shared<StaticMeshData>();
			m_mesh->data->ImportFile("res/meshes/sphere.glb", *m_cmd_pool);

			// Allocate the command buffers used for rendering
			CreateCommandBuffers();

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
			SNK_ASSERT(m_messenger, "Debug messenger created");
		}


	
		void CreateGraphicsPipeline() {
			auto binding_desc = Vertex::GetBindingDescription();
			auto attribute_desc = Vertex::GetAttributeDescriptions();

			PipelineLayoutBuilder layout_builder{};
		/*	layout_builder.AddPushConstant(0, sizeof(glm::mat4), vk::ShaderStageFlagBits::eVertex)
				.AddDescriptorSetLayout(0, m_descriptor_buffers[0].descriptor_spec.GetLayout())
				.AddDescriptorSetLayout(1, m_texture_descriptor_buffer.descriptor_buffers[0].descriptor_spec.GetLayout())
				.AddDescriptorSetLayout(2, m_material_descriptor_buffer.descriptor_buffers[0].descriptor_spec.GetLayout())
				.AddDescriptorSetLayout(3, m_light_descriptor_spec.GetLayout())
				.Build();*/

			//m_pipeline_layout.Init(layout_builder);

			GraphicsPipelineBuilder graphics_builder{};
			graphics_builder.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/vert.spv")
				.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/frag.spv")
				.AddVertexBinding(attribute_desc[0], binding_desc[0])
				.AddVertexBinding(attribute_desc[1], binding_desc[1])
				.AddVertexBinding(attribute_desc[2], binding_desc[2])
				.AddColourAttachment(window.m_vk_context.swapchain_format)
				.AddDepthAttachment(FindDepthFormat())
				//.SetPipelineLayout(m_pipeline_layout.GetPipelineLayout())
				.Build();
			
			m_graphics_pipeline.Init(graphics_builder);
		}


		void CreateCommandPool() {
			QueueFamilyIndices qf_indices = FindQueueFamilies(VulkanContext::GetPhysicalDevice().device, *window.m_vk_context.surface);

			vk::CommandPoolCreateInfo pool_info{};
			pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // allow command buffers under this pool to be rerecorded individually instead of having to be reset together
			pool_info.queueFamilyIndex = qf_indices.graphics_family.value();

			m_cmd_pool = VulkanContext::GetLogicalDevice().device->createCommandPoolUnique(pool_info).value;
			SNK_ASSERT(m_cmd_pool, "Command pool created");
		}

		void CreateCommandBuffers() {
			for (auto& buf : m_cmd_buffers) {
				buf.Init(*m_cmd_pool);
			}
		}

		void RecordCommandBuffer(vk::CommandBuffer cmd_buffer, uint32_t image_index) {
			vk::CommandBufferBeginInfo begin_info{};
			SNK_CHECK_VK_RESULT(
				m_cmd_buffers[m_current_frame].buf->begin(&begin_info)
			);

			Image2D::TransitionImageLayout(window.m_vk_context.swapchain_images[image_index],
				vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal, *m_cmd_pool, cmd_buffer);

			vk::RenderingAttachmentInfo colour_attachment_info{};
			colour_attachment_info.loadOp = vk::AttachmentLoadOp::eClear; // Clear initially
			colour_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
			colour_attachment_info.imageView = *window.m_vk_context.swapchain_image_views[image_index];
			colour_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::RenderingAttachmentInfo depth_attachment_info{};
			depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
			depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
			depth_attachment_info.imageView = m_depth_image.GetImageView();
			depth_attachment_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
			depth_attachment_info.clearValue = vk::ClearValue{ vk::ClearDepthStencilValue{1.f} };

			vk::RenderingInfo render_info{};
			render_info.layerCount = 1;
			render_info.colorAttachmentCount = 1;
			render_info.pColorAttachments = &colour_attachment_info;
			render_info.pDepthAttachment = &depth_attachment_info;
			render_info.renderArea.extent = window.m_vk_context.swapchain_extent;
			render_info.renderArea.offset = vk::Offset2D{ 0, 0 };

			cmd_buffer.beginRenderingKHR(
				render_info
			);

			// Binds shaders and sets all state like rasterizer, blend, multisampling etc
			cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline.GetPipeline());

			vk::Viewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(window.m_vk_context.swapchain_extent.width);
			viewport.height = static_cast<float>(window.m_vk_context.swapchain_extent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			cmd_buffer.setViewport(0, 1, &viewport);
			vk::Rect2D scissor{};
			scissor.offset = vk::Offset2D{ 0, 0 };
			scissor.extent = window.m_vk_context.swapchain_extent;
			cmd_buffer.setScissor(0, 1, &scissor);

			std::vector<vk::Buffer> vert_buffers = { m_mesh->data->position_buf.buffer, m_mesh->data->normal_buf.buffer, m_mesh->data->tex_coord_buf.buffer };
			std::vector<vk::Buffer> index_buffers = { m_mesh->data->index_buf.buffer };
			std::vector<vk::DeviceSize> offsets = { 0, 0, 0 };
			cmd_buffer.bindVertexBuffers(0, 3, vert_buffers.data(), offsets.data());
			cmd_buffer.bindIndexBuffer(m_mesh->data->index_buf.buffer, 0, vk::IndexType::eUint32);

			std::array<vk::DescriptorBufferBindingInfoEXT, 4> binding_infos = { m_matrix_descriptor_buffers[m_current_frame].GetBindingInfo(),
				AssetManager::GetGlobalTexMatBufBindingInfo(m_current_frame), m_light_descriptor_buffers[m_current_frame].GetBindingInfo(), 
				 m_main_pass_descriptor_buffers[m_current_frame].GetBindingInfo()};

			cmd_buffer.bindDescriptorBuffersEXT(binding_infos);

			std::array<uint32_t, 4> buffer_indices = { 0, 1, 2, 3 };
			std::array<vk::DeviceSize, 4> buffer_offsets = { 0, 0, 0, 0 };

			cmd_buffer.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline.pipeline_layout.GetPipelineLayout(), 0, buffer_indices, buffer_offsets);
			for (auto [entity, renderable, transform] : scene.GetRegistry().view<RenderableComponent, TransformComponent>().each()) {
				cmd_buffer.pushConstants(m_graphics_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(glm::mat4), &transform.GetMatrix()[0][0]);
				cmd_buffer.drawIndexed((uint32_t)m_mesh->data->index_buf.alloc_info.size / sizeof(uint32_t), 1, 0, 0, 0);
			}
		
			cmd_buffer.endRenderingKHR();

			Image2D::TransitionImageLayout(window.m_vk_context.swapchain_images[image_index],
				vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eColorAttachmentOptimal,
				vk::ImageLayout::ePresentSrcKHR, *m_cmd_pool, cmd_buffer);


			SNK_CHECK_VK_RESULT(
				cmd_buffer.end()
			);
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

			SNK_ASSERT(m_image_avail_semaphores[0] && m_render_finished_semaphores[0] && m_in_flight_fences[0], "Synchronization objects created");
		}

		void RenderLoop() {
			while (!glfwWindowShouldClose(window.p_window)) {
				window.input.OnUpdate(window.p_window);
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

				p_transform->SetPosition(p_transform->GetPosition() + move);

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

			UpdateUniformBuffer(m_current_frame);

			m_cmd_buffers[m_current_frame].buf->reset();
			RecordCommandBuffer(*m_cmd_buffers[m_current_frame].buf, image_index);
			m_shadow_cmd_buffers[m_current_frame].buf->reset();
			RecordShadowCmdBuffers();

			vk::SubmitInfo depth_submit_info;
			depth_submit_info.setCommandBufferCount(1)
				.setPCommandBuffers(&*m_shadow_cmd_buffers[m_current_frame].buf);

			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().graphics_queue.submit(depth_submit_info)
			);

			vk::SubmitInfo submit_info{};
			auto wait_semaphores = util::array(*m_image_avail_semaphores[m_current_frame]); // Which semaphores to wait on
			auto wait_stages = util::array<vk::PipelineStageFlags>(vk::PipelineStageFlagBits::eColorAttachmentOutput);

			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = wait_semaphores.data();
			submit_info.pWaitDstStageMask = wait_stages.data();
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &*m_cmd_buffers[m_current_frame].buf;

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
				m_depth_image.DestroyImage();
				CreateDepthResources();
			}
			else if (present_result != vk::Result::eSuccess) {
				SNK_BREAK("Failed to present image");
			}

			m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
		}

		void CreateDescriptorBuffers() {
			auto& device = VulkanContext::GetLogicalDevice().device;
			auto& descriptor_buffer_properties = VulkanContext::GetPhysicalDevice().buffer_properties;

			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				auto matrix_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
				matrix_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAllGraphics)
					.GenDescriptorLayout();

				auto light_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
				light_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAllGraphics)
					.GenDescriptorLayout();

				auto main_pass_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
				main_pass_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAllGraphics)
					.GenDescriptorLayout();

				m_matrix_descriptor_buffers[i].SetDescriptorSpec(matrix_descriptor_set_spec);
				m_light_descriptor_buffers[i].SetDescriptorSpec(light_descriptor_set_spec);
				m_main_pass_descriptor_buffers[i].SetDescriptorSpec(main_pass_descriptor_set_spec);
				
				m_main_pass_descriptor_buffers[i].CreateBuffer(1);
				m_matrix_descriptor_buffers[i].CreateBuffer(1);
				m_light_descriptor_buffers[i].CreateBuffer(1);

				auto mat_info = m_matrix_ubos[i].CreateDescriptorGetInfo();
				m_matrix_descriptor_buffers[i].LinkResource(mat_info.first, 0, 0);

				auto shadow_map_info = m_shadow_depth_image.CreateDescriptorGetInfo(vk::ImageLayout::eShaderReadOnlyOptimal);
				m_main_pass_descriptor_buffers[i].LinkResource(shadow_map_info.first, 0, 0);

				auto light_info = m_light_ubos[i].CreateDescriptorGetInfo();
				m_light_descriptor_buffers[i].LinkResource(light_info.first, 0, 0);
			}
		}


		void CreateUniformBuffers() {
			vk::DeviceSize matrix_buffer_size = sizeof(UBO);
			// Adding 8 floats for alignment
			 constexpr uint32_t LIGHT_UBO_SIZE = sizeof(float) * 24 + sizeof(float) * 8;

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_matrix_ubos[i].CreateBuffer(matrix_buffer_size, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
				m_light_ubos[i].CreateBuffer(LIGHT_UBO_SIZE, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
			}
		}

		struct LightUBO {
			alignas(16) glm::vec4 colour;
			alignas(16) glm::vec4 dir;
			alignas(16) glm::mat4 light_transform;
		};

		void UpdateUniformBuffer(uint32_t current_frame) {
			auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();

			UBO ubo{};
			auto p = p_transform->GetPosition();

			ubo.view = glm::lookAt(p_transform->GetPosition(), p_transform->GetPosition() + p_transform->forward, glm::vec3(0.f, 1.f, 0.f));
			ubo.proj = glm::perspective(glm::radians(45.f), window.m_vk_context.swapchain_extent.width / (float)window.m_vk_context.swapchain_extent.height, 0.01f, 10000.f);

			// Y coordinate of clip coordinates inverted as glm designed to work with opengl, flip here
			ubo.proj[1][1] *= -1;

			memcpy(m_matrix_ubos[current_frame].Map(), &ubo, sizeof(UBO));

			LightUBO light_ubo;
			light_ubo.colour = glm::vec4(1, 0, 0, 1);
			light_ubo.dir = glm::vec4(1, 0, 0, 1);
			
			auto ortho = glm::ortho(-10.f, 10.f, -10.f, 10.f, 0.1f, 10.f);
			auto view = glm::lookAt(glm::vec3{ -5, 0, 0 }, glm::vec3{ 0, 0, 0 }, glm::vec3{ 0, 1, 0 });
			light_ubo.light_transform = ortho * view;

			memcpy(m_light_ubos[m_current_frame].Map(), &light_ubo, sizeof(light_ubo));
		}


		vk::Format FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
			for (auto format : candidates) {
				vk::FormatProperties properties = VulkanContext::GetPhysicalDevice().device.getFormatProperties(format);
				if (tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) == features) {
					return format;
				}
				else if (tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features) {
					return format;
				}
			}

			SNK_BREAK("No supported format found");
			return vk::Format::eUndefined;
		}

		bool HasStencilComponent(vk::Format format) {
			return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
		}

		vk::Format FindDepthFormat() {
			return FindSupportedFormat({ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
				vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
		}

		void CreateDepthResources() {
			auto depth_format = FindDepthFormat();

			Image2DSpec depth_spec{};
			depth_spec.format = depth_format;
			depth_spec.size = { window.m_width, window.m_height };
			depth_spec.tiling = vk::ImageTiling::eOptimal;
			depth_spec.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

			m_depth_image.SetSpec(depth_spec);
			m_depth_image.CreateImage();
			m_depth_image.CreateImageView(vk::ImageAspectFlagBits::eDepth);

			m_depth_image.TransitionImageLayout(m_depth_image.GetImage(), 
				vk::ImageAspectFlagBits::eDepth | (HasStencilComponent(depth_format) ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eNone ), 
				vk::ImageLayout::eUndefined,
				(HasStencilComponent(depth_format) ? vk::ImageLayout::eDepthAttachmentOptimal: vk::ImageLayout::eDepthAttachmentOptimal), *m_cmd_pool);
		}

		~VulkanApp() {
			// Wait for any fences before destroying them
			for (auto& fence : m_in_flight_fences) {
				SNK_CHECK_VK_RESULT(
					VulkanContext::GetLogicalDevice().device->waitForFences(*fence, true, std::numeric_limits<uint64_t>::max())
				)
			}
		}

		void SetupShadowMapping() {
			Image2DSpec shadow_spec;
			shadow_spec.size = { 4096, 4096 };
			shadow_spec.format = vk::Format::eD16Unorm;
			shadow_spec.tiling = vk::ImageTiling::eOptimal;
			shadow_spec.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

			m_shadow_depth_image.SetSpec(shadow_spec);
			m_shadow_depth_image.CreateImage();
			m_shadow_depth_image.CreateImageView(vk::ImageAspectFlagBits::eDepth);
			m_shadow_depth_image.CreateSampler();

			Image2D::TransitionImageLayout(m_shadow_depth_image.GetImage(),
				vk::ImageAspectFlagBits::eDepth, vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal, *m_cmd_pool, vk::AccessFlagBits::eNone,
				vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

			PipelineLayoutBuilder layout_builder{};
			DescriptorSetSpec spec{};
			spec.GenDescriptorLayout();

			auto vert_binding_descs = Vertex::GetBindingDescription();
			auto vert_attr_descs = Vertex::GetAttributeDescriptions();

			GraphicsPipelineBuilder builder{};
			builder.AddDepthAttachment(vk::Format::eD16Unorm)
				.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/depth_vert.spv")
				.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/depth_frag.spv")
				.AddVertexBinding(vert_attr_descs[0], vert_binding_descs[0])
				.AddVertexBinding(vert_attr_descs[1], vert_binding_descs[1])
				.AddVertexBinding(vert_attr_descs[2], vert_binding_descs[2])
				//.SetPipelineLayout(m_shadow_pipeline_layout.GetPipelineLayout())
				.Build();

			m_shadow_pipeline.Init(builder);

			for (auto& buf : m_shadow_cmd_buffers) {
				buf.Init(*m_cmd_pool);
			}


		}

		void RecordShadowCmdBuffers() {
			auto cmd = *m_shadow_cmd_buffers[m_current_frame].buf;
			vk::CommandBufferBeginInfo begin_info{};
			cmd.begin(&begin_info);

			Image2D::TransitionImageLayout(m_shadow_depth_image.GetImage(),
				vk::ImageAspectFlagBits::eDepth, vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eDepthAttachmentOptimal, *m_cmd_pool, vk::AccessFlagBits::eShaderRead, 
				vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, cmd);
	

			vk::RenderingAttachmentInfo depth_info;
			depth_info.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
				.setImageView(m_shadow_depth_image.GetImageView())
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setLoadOp(vk::AttachmentLoadOp::eClear)
				.setClearValue(vk::ClearDepthStencilValue{ 1.f });
			

			vk::RenderingInfo render_info{};
			render_info.layerCount = 1;
			render_info.pDepthAttachment = &depth_info;
			render_info.renderArea.extent = vk::Extent2D{ 4096, 4096 };
			render_info.renderArea.offset = vk::Offset2D{ 0, 0 };

			cmd.beginRenderingKHR(
				render_info
			);

			// Binds shaders and sets all state like rasterizer, blend, multisampling etc
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_shadow_pipeline.GetPipeline());


			vk::Viewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = 4096;
			viewport.height = 4096;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			cmd.setViewport(0, 1, &viewport);
			vk::Rect2D scissor{};
			scissor.offset = vk::Offset2D{ 0, 0 };
			scissor.extent = vk::Extent2D(4096,4096);
			cmd.setScissor(0, 1, &scissor);

			std::vector<vk::Buffer> vert_buffers = { m_mesh->data->position_buf.buffer, m_mesh->data->normal_buf.buffer, m_mesh->data->tex_coord_buf.buffer };
			std::vector<vk::Buffer> index_buffers = { m_mesh->data->index_buf.buffer };
			std::vector<vk::DeviceSize> offsets = { 0, 0, 0 };
			cmd.bindVertexBuffers(0, 3, vert_buffers.data(), offsets.data());
			cmd.bindIndexBuffer(m_mesh->data->index_buf.buffer, 0, vk::IndexType::eUint32);

			vk::DescriptorBufferBindingInfoEXT light_buffer_binding_info{};
			light_buffer_binding_info.address = m_light_descriptor_buffers[m_current_frame].descriptor_buffer.GetDeviceAddress();
			light_buffer_binding_info.usage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT;

			cmd.bindDescriptorBuffersEXT(light_buffer_binding_info);
			std::array<uint32_t, 1> buffer_indices = { 0 };
			std::array<vk::DeviceSize, 1> buffer_offsets = { 0 };

			cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_shadow_pipeline.pipeline_layout.GetPipelineLayout(), (uint32_t)DescriptorSetIndices::LIGHTS, buffer_indices, buffer_offsets);

			for (auto [entity, renderable, transform] : scene.GetRegistry().view<RenderableComponent, TransformComponent>().each()) {
				cmd.pushConstants(m_shadow_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(glm::mat4), &transform.GetMatrix()[0][0]);
				cmd.drawIndexed((uint32_t)m_mesh->data->index_buf.alloc_info.size / sizeof(uint32_t), 1, 0, 0, 0);
			}

			cmd.endRenderingKHR();

			Image2D::TransitionImageLayout(m_shadow_depth_image.GetImage(),
				vk::ImageAspectFlagBits::eDepth, vk::ImageLayout::eDepthAttachmentOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal, *m_cmd_pool, vk::AccessFlagBits::eDepthStencilAttachmentWrite, 
				vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eLateFragmentTests, vk::PipelineStageFlagBits::eBottomOfPipe, cmd);

			SNK_CHECK_VK_RESULT(cmd.end());
		}

		SNAKE::Window window;
	
	private:
		Scene scene;
		Entity* p_cam_ent = nullptr;

		AssetRef<MaterialAsset> m_material{nullptr};
		AssetRef<MaterialAsset> m_material_2{ nullptr };
		AssetRef<StaticMeshAsset> m_mesh{ nullptr };
		AssetRef<Texture2DAsset> m_tex{ nullptr };

		EventListener m_framebuffer_resize_listener;
	
		Image2D m_depth_image;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_matrix_ubos;

		// TODO: Move to scene camera system
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_matrix_descriptor_buffers;

		vk::UniqueDebugUtilsMessengerEXT m_messenger;

		GraphicsPipeline m_graphics_pipeline;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_light_ubos;

		GraphicsPipeline m_shadow_pipeline;

		Image2D m_shadow_depth_image;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_shadow_cmd_buffers;

		// TODO: Move to a scene lighting system/manager
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_light_descriptor_buffers;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_main_pass_descriptor_buffers;


		vk::UniqueCommandPool m_cmd_pool;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
		std::vector<vk::UniqueSemaphore> m_image_avail_semaphores;
		std::vector<vk::UniqueSemaphore> m_render_finished_semaphores;
		std::vector<vk::UniqueFence> m_in_flight_fences;

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
	std::filesystem::current_path("../../../../Core");
	SNAKE::Logger::Init();

	SNAKE::VulkanApp app{};
	app.Init("Snake");
	app.RenderLoop();

	return 0;
}