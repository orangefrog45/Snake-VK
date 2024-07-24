#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "util/FileUtil.h"
#include "util/util.h"
#include "core/Window.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"
#include "core/VkIncl.h"
#include "textures/Textures.h"

#include <stb_image.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <filesystem>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace SNAKE;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;


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
	Vertex(glm::vec2 _pos, glm::vec3 _col, glm::vec2 _tex_coord) : pos(_pos), colour(_col), tex_coord(_tex_coord) {};
	glm::vec2 pos;
	glm::vec3 colour;
	glm::vec2 tex_coord;

	// Struct which describes how to move through the data provided
	[[nodiscard]] constexpr inline static vk::VertexInputBindingDescription GetBindingDescription() {
		vk::VertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = vk::VertexInputRate::eVertex;

		return desc;
	}

	// Struct which describes vertex attribute access in shaders
	[[nodiscard]] constexpr inline static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions() {
		std::array<vk::VertexInputAttributeDescription, 3> descs{};

		descs[0].binding = 0;
		descs[0].location = 0;
		descs[0].format = vk::Format::eR32G32Sfloat;
		descs[0].offset = offsetof(Vertex, pos);

		descs[1].binding = 0;
		descs[1].location = 1;
		descs[1].format = vk::Format::eR32G32B32Sfloat;
		descs[1].offset = offsetof(Vertex, colour);

		descs[2].binding = 0;
		descs[2].location = 2;
		descs[2].format = vk::Format::eR32G32Sfloat;
		descs[2].offset = offsetof(Vertex, tex_coord);

		return descs;
	}
};

struct UBO {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
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

			// Create common descriptor set layout, includes bindings for UBO object used in CreateGraphicsPipeline and CreateDescriptorSets
			CreateDescriptorSetLayout();

			// Create a pipeline with shaders, descriptor sets (in pipeline layout), rasterizer/blend/dynamic state
			CreateGraphicsPipeline();

			// Create command pool which can allocate command buffers
			CreateCommandPool();

			m_tex.LoadFromFile("res/textures/oranges_opt.jpg", *m_cmd_pool);

			// Create vertex buffer, uses staging buffer to transfer contents into more efficient gpu memory
			CreateVertexBuffer();
			CreateIndexBuffer();

			// Create persistently mapped uniform buffers and store the ptrs which can have data copied to them from the host
			CreateUniformBuffers();

			// Descriptor pool object can allocate descriptor sets
			CreateDescriptorPool();

			// Allocate descriptor sets and connect the uniform buffers to them
			CreateDescriptorSets();

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
			constexpr char vert_fp[] = "res/shaders/vert.spv";
			constexpr char frag_fp[] = "res/shaders/frag.spv";

			auto vert_code = files::ReadFileBinary(vert_fp);
			auto frag_code = files::ReadFileBinary(frag_fp);

			SNK_ASSERT(!vert_code.empty(), "Vertex shader code loaded");
			SNK_ASSERT(!frag_code.empty(), "Fragment shader code loaded");

			auto vert_module = CreateShaderModule(vert_code, vert_fp);
			auto frag_module = CreateShaderModule(frag_code, frag_fp);

			vk::PipelineShaderStageCreateInfo vert_info{ vk::PipelineShaderStageCreateFlags{0}, vk::ShaderStageFlagBits::eVertex, vert_module.get(), "main"};
			vk::PipelineShaderStageCreateInfo frag_info{ vk::PipelineShaderStageCreateFlags{0}, vk::ShaderStageFlagBits::eFragment, frag_module.get(), "main"};
			vk::PipelineShaderStageCreateInfo shader_stages[] = {vert_info, frag_info};

			auto binding_desc = Vertex::GetBindingDescription();
			auto attribute_desc = Vertex::GetAttributeDescriptions();

			// Stores vertex layout data
			vk::PipelineVertexInputStateCreateInfo vert_input_info{ vk::PipelineVertexInputStateCreateFlags{0}, nullptr, {} };
			vert_input_info.vertexBindingDescriptionCount = 1;
			vert_input_info.vertexAttributeDescriptionCount = (uint32_t)attribute_desc.size();
			vert_input_info.pVertexBindingDescriptions = &binding_desc;
			vert_input_info.pVertexAttributeDescriptions = attribute_desc.data();

			// Stores how vertex data is interpreted by input assembler
			vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info{ 
				vk::PipelineInputAssemblyStateCreateFlags{0}, vk::PrimitiveTopology::eTriangleList, VK_FALSE };

			std::vector<vk::DynamicState> dynamic_states = {
				vk::DynamicState::eViewport,
				vk::DynamicState::eScissor
			};

			vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{ vk::PipelineDynamicStateCreateFlags{0}, dynamic_states };

			vk::PipelineViewportStateCreateInfo viewport_create_info{}; // Don't set pviewport or pscissor to keep this dynamic
			viewport_create_info.viewportCount = 1;
			viewport_create_info.scissorCount = 1;

			vk::PipelineRasterizationStateCreateInfo rasterizer_create_info{};
			rasterizer_create_info.depthClampEnable = VK_FALSE; // if fragments beyond near and far plane are clamped to them instead of discarding
			rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE; // if geometry should never pass through rasterizer stage
			rasterizer_create_info.polygonMode = vk::PolygonMode::eFill;
			rasterizer_create_info.lineWidth = 1.f;
			rasterizer_create_info.cullMode = vk::CullModeFlagBits::eBack;
			rasterizer_create_info.frontFace = vk::FrontFace::eCounterClockwise;
			rasterizer_create_info.depthBiasEnable = VK_FALSE;

			vk::PipelineMultisampleStateCreateInfo multisampling{};
			multisampling.sampleShadingEnable = VK_FALSE;
			multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
			multisampling.minSampleShading = 1.f;

			vk::PipelineColorBlendAttachmentState colour_blend_attachment{};
			// Enable alpha blending
			colour_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			colour_blend_attachment.blendEnable = VK_TRUE;
			colour_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
			colour_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
			colour_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
			colour_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
			colour_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
			colour_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;

			vk::PipelineColorBlendStateCreateInfo colour_blend_state;
			colour_blend_state.logicOpEnable = VK_FALSE;
			colour_blend_state.attachmentCount = 1;
			colour_blend_state.pAttachments = &colour_blend_attachment;
		
			// Use to attach descriptor sets
			vk::PipelineLayoutCreateInfo pipeline_layout_info{};
			pipeline_layout_info.setLayoutCount = 1;
			pipeline_layout_info.pSetLayouts = &*m_descriptor_set_layout;

			m_pipeline_layout = VulkanContext::GetLogicalDevice().device->createPipelineLayoutUnique(pipeline_layout_info).value;

			SNK_ASSERT(m_pipeline_layout, "Graphics pipeline layout created");

			std::vector<vk::Format> formats = { window.m_vk_context.swapchain_format };
			vk::PipelineRenderingCreateInfo render_info{};
			render_info.colorAttachmentCount = 1;
			render_info.pColorAttachmentFormats = formats.data();

			vk::GraphicsPipelineCreateInfo pipeline_info{};
			pipeline_info.stageCount = 2;
			pipeline_info.pStages = shader_stages;
			pipeline_info.pVertexInputState = &vert_input_info;
			pipeline_info.pInputAssemblyState = &input_assembly_create_info;
			pipeline_info.pViewportState = &viewport_create_info;
			pipeline_info.pRasterizationState = &rasterizer_create_info;
			pipeline_info.pMultisampleState = &multisampling;
			pipeline_info.pDepthStencilState = nullptr;
			pipeline_info.pColorBlendState = &colour_blend_state;
			pipeline_info.pDynamicState = &dynamic_state_create_info;
			pipeline_info.layout = *m_pipeline_layout;
			pipeline_info.renderPass = nullptr;
			pipeline_info.subpass = 0; // index of subpass where this pipeline is used
			pipeline_info.pNext = &render_info;

			auto [result, pipeline] = VulkanContext::GetLogicalDevice().device->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipeline_info).asTuple();
			m_graphics_pipeline = std::move(pipeline);

			SNK_CHECK_VK_RESULT(result);
			SNK_ASSERT(m_graphics_pipeline, "Graphics pipeline successfully created");
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
			vk::CommandBufferAllocateInfo alloc_info{};
			alloc_info.commandPool = *m_cmd_pool;
			alloc_info.level = vk::CommandBufferLevel::ePrimary; // can be submitted to a queue for execution, can't be called by other command buffers

			m_cmd_buffers.resize(MAX_FRAMES_IN_FLIGHT);
			alloc_info.commandBufferCount = (uint32_t)m_cmd_buffers.size();
		
			m_cmd_buffers = std::move(VulkanContext::GetLogicalDevice().device->allocateCommandBuffersUnique(alloc_info).value);
			SNK_ASSERT(m_cmd_buffers[0], "Command buffers created");
		}

		void RecordCommandBuffer(vk::CommandBuffer cmd_buffer, uint32_t image_index) {
			vk::CommandBufferBeginInfo begin_info{};

			SNK_CHECK_VK_RESULT(
				m_cmd_buffers[m_current_frame]->begin(begin_info)
			);

			// Transition image layout from presentSrc to colour
			vk::ImageMemoryBarrier image_mem_barrier{};
			image_mem_barrier.oldLayout = vk::ImageLayout::eUndefined;
			image_mem_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
			image_mem_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			image_mem_barrier.image = window.m_vk_context.swapchain_images[image_index];
			image_mem_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			image_mem_barrier.subresourceRange.baseMipLevel = 0;
			image_mem_barrier.subresourceRange.levelCount = 1;
			image_mem_barrier.subresourceRange.baseArrayLayer = 0;
			image_mem_barrier.subresourceRange.layerCount = 1;

			cmd_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::DependencyFlags{ 0 }, {}, {}, image_mem_barrier
			);

			vk::RenderingAttachmentInfo colour_attachment_info{};
			colour_attachment_info.loadOp = vk::AttachmentLoadOp::eClear; // Clear initially
			colour_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
			colour_attachment_info.imageView = *window.m_vk_context.swapchain_image_views[image_index];
			colour_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::RenderingInfo render_info{};
			render_info.layerCount = 1;
			render_info.colorAttachmentCount = 1;
			render_info.pColorAttachments = &colour_attachment_info;
			render_info.renderArea.extent = window.m_vk_context.swapchain_extent;
			render_info.renderArea.offset = vk::Offset2D{ 0, 0 };


			cmd_buffer.beginRenderingKHR(
				render_info
			);

			cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphics_pipeline);

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

			std::vector<vk::Buffer> vert_buffers = { m_vertex_buffer.buffer };
			std::vector<vk::Buffer> index_buffers = { m_index_buffer.buffer };
			std::vector<vk::DeviceSize> offsets = { 0 };
			cmd_buffer.bindVertexBuffers(0, 1, vert_buffers.data(), offsets.data());
			cmd_buffer.bindIndexBuffer(m_index_buffer.buffer, 0, vk::IndexType::eUint16);

			cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipeline_layout, 0, *m_descriptor_sets[m_current_frame], {});
			cmd_buffer.drawIndexed((uint32_t)m_indices.size(), 1, 0, 0, 0);

			cmd_buffer.endRenderingKHR();

			// Transition image layout from colour to presentSrc
			vk::ImageMemoryBarrier image_mem_barrier_2{};
			image_mem_barrier_2.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			image_mem_barrier_2.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
			image_mem_barrier_2.newLayout = vk::ImageLayout::ePresentSrcKHR;
			image_mem_barrier_2.image = window.m_vk_context.swapchain_images[image_index];
			image_mem_barrier_2.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			image_mem_barrier_2.subresourceRange.baseMipLevel = 0;
			image_mem_barrier_2.subresourceRange.levelCount = 1;
			image_mem_barrier_2.subresourceRange.baseArrayLayer = 0;
			image_mem_barrier_2.subresourceRange.layerCount = 1;

			cmd_buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlags{ 0 }, {}, {}, image_mem_barrier_2
			);


			SNK_CHECK_VK_RESULT(
				cmd_buffer.end()
			);
		}

		vk::UniqueShaderModule CreateShaderModule(const std::vector<char>& code, const std::string& filepath) {
			vk::ShaderModuleCreateInfo create_info{
				vk::ShaderModuleCreateFlags{0},
				code.size(), reinterpret_cast<const uint32_t*>(code.data())
			};

			vk::UniqueShaderModule shader_module = VulkanContext::GetLogicalDevice().device->createShaderModuleUnique(create_info).value;

			SNK_ASSERT(shader_module, "Shader module for shader '{0}' created successfully", filepath)
			return shader_module;
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
				DrawFrame();
				glfwPollEvents();
			}

			glfwTerminate();
		}

		void DrawFrame() {
			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().device->waitForFences(1, &*m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX)
			);

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

			m_cmd_buffers[m_current_frame]->reset();
			RecordCommandBuffer(*m_cmd_buffers[m_current_frame], image_index);

			vk::SubmitInfo submit_info{};
			auto wait_semaphores = util::array(*m_image_avail_semaphores[m_current_frame]); // Which semaphores to wait on
			auto wait_stages = util::array<vk::PipelineStageFlags>(vk::PipelineStageFlagBits::eColorAttachmentOutput);

			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = wait_semaphores.data();
			submit_info.pWaitDstStageMask = wait_stages.data();
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &*m_cmd_buffers[m_current_frame];

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
			}
			else if (present_result != vk::Result::eSuccess) {
				SNK_BREAK("Failed to present image");
			}

			m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
		}

		void CreateVertexBuffer() {
			const std::vector<Vertex> vertices = {
				{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
				{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
				{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
				{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
			};


			size_t size = vertices.size() * sizeof(Vertex);

			S_VkBuffer staging_buffer;
			staging_buffer.CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

			void* data = nullptr;
			SNK_CHECK_VK_RESULT(vmaMapMemory(VulkanContext::GetAllocator(), staging_buffer.allocation, &data));
			memcpy(data, vertices.data(), size);
			vmaUnmapMemory(VulkanContext::GetAllocator(), staging_buffer.allocation);

			m_vertex_buffer.CreateBuffer(size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer);

			CopyBuffer(staging_buffer.buffer, m_vertex_buffer.buffer, size);
		}

		void CreateIndexBuffer() {
			size_t size = m_indices.size() * sizeof(Vertex);

			S_VkBuffer staging_buffer;
			staging_buffer.CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

			void* data = nullptr;

			SNK_CHECK_VK_RESULT(vmaMapMemory(VulkanContext::GetAllocator(), staging_buffer.allocation, &data));
			memcpy(data, m_indices.data(), size);
			vmaUnmapMemory(VulkanContext::GetAllocator(), staging_buffer.allocation);

			m_index_buffer.CreateBuffer(size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer);

			CopyBuffer(staging_buffer.buffer, m_index_buffer.buffer, size);
		}

		void CopyBuffer(vk::Buffer& src, vk::Buffer dst, vk::DeviceSize size) {
			vk::UniqueCommandBuffer cmd_buf = BeginSingleTimeCommands(*m_cmd_pool);

			vk::BufferCopy copy_region{};
			copy_region.srcOffset = 0;
			copy_region.dstOffset = 0;
			copy_region.size = size;

			cmd_buf->copyBuffer(src, dst, copy_region);

			EndSingleTimeCommands(*cmd_buf);
		}

		void CreateDescriptorSetLayout() {
			// Binding for the UBO inside the descriptor set, positioned at index 0 (binding = 0), as written in vertex shader
			vk::DescriptorSetLayoutBinding ubo_layout_binding{};
			ubo_layout_binding.binding = 0;
			ubo_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
			ubo_layout_binding.descriptorCount = 1;
			ubo_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex; // Descriptor only referenced in vertex stage

			vk::DescriptorSetLayoutBinding sampler_layout_binding{};
			sampler_layout_binding.binding = 1;
			sampler_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			sampler_layout_binding.descriptorCount = 1;
			sampler_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment; // Descriptor only referenced in fragment stage
			
			auto bindings = util::array(ubo_layout_binding, sampler_layout_binding);
			vk::DescriptorSetLayoutCreateInfo layout_info{};
			layout_info.bindingCount = (uint32_t)bindings.size();
			layout_info.pBindings = bindings.data();

			auto [res, val] = VulkanContext::GetLogicalDevice().device->createDescriptorSetLayoutUnique(layout_info);
			SNK_CHECK_VK_RESULT(res);
	
			m_descriptor_set_layout = std::move(val);
		}

		void CreateUniformBuffers() {
			vk::DeviceSize buffer_size = sizeof(UBO);
		
			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_uniform_buffers[i].CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
				// Mapped ptr is valid for the rest of the application
				SNK_CHECK_VK_RESULT(vmaMapMemory(VulkanContext::GetAllocator(), m_uniform_buffers[i].allocation, &m_uniform_buffers_mapped[i]));
			}
		}

		void UpdateUniformBuffer(uint32_t current_image) {
			static auto start_time = std::chrono::high_resolution_clock::now();

			auto current_time = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

			UBO ubo{};
			ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
			ubo.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
			ubo.proj = glm::perspective(glm::radians(45.f), window.m_vk_context.swapchain_extent.width / (float)window.m_vk_context.swapchain_extent.height, 0.01f, 10.f);

			// Y coordinate of clip coordinates inverted as glm designed to work with opengl, flip here
			ubo.proj[1][1] *= -1;

			memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(UBO));
		}

		void CreateDescriptorPool() {
			std::array<vk::DescriptorPoolSize, 2> sizes;

			sizes[0].type = vk::DescriptorType::eUniformBuffer;
			sizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
			sizes[1].type = vk::DescriptorType::eCombinedImageSampler;
			sizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

			vk::DescriptorPoolCreateInfo pool_info{};
			pool_info.poolSizeCount = 2;
			pool_info.pPoolSizes = sizes.data();
			pool_info.maxSets = MAX_FRAMES_IN_FLIGHT;

			auto [res, pool] = VulkanContext::GetLogicalDevice().device->createDescriptorPoolUnique(pool_info);
			SNK_CHECK_VK_RESULT(res);

			m_descriptor_pool = std::move(pool);
		}

		void CreateDescriptorSets() {
			// Allocate descriptor sets from pool
			auto layouts = std::vector<vk::DescriptorSetLayout>(MAX_FRAMES_IN_FLIGHT, *m_descriptor_set_layout);
			vk::DescriptorSetAllocateInfo alloc_info{};
			alloc_info.descriptorPool = *m_descriptor_pool;
			alloc_info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
			alloc_info.pSetLayouts = layouts.data();

			vk::DescriptorImageInfo image_info{};
			image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			image_info.imageView = m_tex.GetImageView();
			image_info.sampler = m_tex.GetSampler();

			m_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
			auto [res, sets] = VulkanContext::GetLogicalDevice().device->allocateDescriptorSetsUnique(alloc_info);
			SNK_CHECK_VK_RESULT(res);

			// Connect buffers to descriptor sets
			for (size_t i = 0; i < sets.size(); i++) {
				m_descriptor_sets[i] = std::move(sets[i]);

				vk::DescriptorBufferInfo buffer_info{};
				buffer_info.buffer = m_uniform_buffers[i].buffer;
				buffer_info.offset = 0;
				buffer_info.range = sizeof(UBO);

				vk::WriteDescriptorSet ubo_descriptor_write{};
				ubo_descriptor_write.dstSet = *m_descriptor_sets[i];
				ubo_descriptor_write.dstBinding = 0;
				ubo_descriptor_write.dstArrayElement = 0;
				ubo_descriptor_write.descriptorType = vk::DescriptorType::eUniformBuffer;
				ubo_descriptor_write.pBufferInfo = &buffer_info; // Leave pImageInfo and pTexelBufferView fields empty as this descriptor refers to just buffer data
				ubo_descriptor_write.descriptorCount = 1;

				vk::WriteDescriptorSet image_descriptor_write{};
				image_descriptor_write.dstSet = *m_descriptor_sets[i];
				image_descriptor_write.dstBinding = 1;
				image_descriptor_write.dstArrayElement = 0;
				image_descriptor_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
				image_descriptor_write.descriptorCount = 1;
				image_descriptor_write.pImageInfo = &image_info;

				auto descriptor_writes = util::array(ubo_descriptor_write, image_descriptor_write);

				VulkanContext::GetLogicalDevice().device->updateDescriptorSets(descriptor_writes, {});
			}

		}

		~VulkanApp() {
			// Wait for any fences before destroying them
			for (auto& fence : m_in_flight_fences) {
				SNK_CHECK_VK_RESULT(
					VulkanContext::GetLogicalDevice().device->waitForFences(*fence, true, std::numeric_limits<uint64_t>::max())
				)
			}
		}

		const std::vector<uint16_t> m_indices = {
			0, 1, 2, 2, 3, 0
		};

		SNAKE::Window window;
	
	private:
		EventListener m_framebuffer_resize_listener;

		Texture2D m_tex;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_uniform_buffers;
		std::array<void*, MAX_FRAMES_IN_FLIGHT> m_uniform_buffers_mapped;

		S_VkBuffer m_vertex_buffer;

		S_VkBuffer m_index_buffer;

		vk::UniqueDebugUtilsMessengerEXT m_messenger;

		vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
		vk::UniqueDescriptorPool m_descriptor_pool;
		std::vector<vk::UniqueDescriptorSet> m_descriptor_sets;

		vk::UniquePipelineLayout m_pipeline_layout;

		vk::UniquePipeline m_graphics_pipeline;

		vk::UniqueCommandPool m_cmd_pool;
		std::vector<vk::UniqueCommandBuffer> m_cmd_buffers;

		std::vector<vk::UniqueSemaphore> m_image_avail_semaphores;
		std::vector<vk::UniqueSemaphore> m_render_finished_semaphores;
		std::vector<vk::UniqueFence> m_in_flight_fences;

		bool m_framebuffer_resized = false;

		uint32_t m_current_frame = 0;

		std::vector<const char*> m_required_device_extensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
			VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
			VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
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