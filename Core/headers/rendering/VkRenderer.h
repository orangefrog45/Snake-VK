#pragma once
#include "core/Window.h"
#include "util/util.h"
#include "core/VkCommands.h"
#include "core/Pipelines.h"

namespace SNAKE {
	struct SwapchainInvalidateEvent : public Event {
		SwapchainInvalidateEvent(glm::vec2 new_extents) : new_swapchain_extents(new_extents) {};
		glm::vec2 new_swapchain_extents{ 0, 0 };
	};

	struct Line {
		Line(glm::vec4 c, glm::vec3 _p0, glm::vec3 _p1) : colour(c), p0(_p0), p1(_p1) {};
		glm::vec4 colour;
		glm::vec3 p0;
		glm::vec3 p1;
	};

	struct Sphere {
		Sphere(glm::vec4 c, glm::vec3 p, float radius) : colour(c), pos(p), r(radius) {};
		glm::vec4 colour;
		glm::vec3 pos;
		float r;
	};

	class VkRenderer {
	public:
		static VkRenderer& Get() {
			static VkRenderer instance;
			return instance;
		}

		static void Init() {
			Get().InitImpl();
		}

		static void QueueDebugRenderLine(glm::vec3 pos0, glm::vec3 pos1, glm::vec4 colour) {
			Get().m_render_data.lines.emplace_back(colour, pos0, pos1);
		}

		static void RecordRenderDebugCommands(vk::CommandBuffer cmd_buf, Image2D& colour_image, Image2D& depth_image, DescriptorBuffer& scene_data_db) {
			Get().RecordRenderDebugCommandsImpl(cmd_buf, colour_image, depth_image, scene_data_db);
		}

		// Resulting swapchain image index is written into image_index
		static vk::Semaphore AcquireNextSwapchainImage(Window& window, uint32_t& image_index) {
			return Get().AcquireNextSwapchainImageImpl(window, image_index);
		}

		static void Shutdown() {
			for (FrameInFlightIndex i = 0; i < Get().m_in_flight_fences.size(); i++) {
				SNK_CHECK_VK_RESULT(VulkanContext::GetLogicalDevice().device->waitForFences(1, &*Get().m_in_flight_fences[i], VK_TRUE, UINT64_MAX));
				SNK_CHECK_VK_RESULT(VulkanContext::GetLogicalDevice().device->resetFences(1, &*Get().m_in_flight_fences[i]));
			}
		}

		static uint32_t GetCurrentSwapchainImageIndex() {
			return Get().m_current_swapchain_image_index;
		}

		// render_image should be in layout "ColorAttachmentOptimal" and have an image view
		// This function also triggers the frame fence and so should always be called last after all renderpasses
		static void RenderImGuiAndPresent(Window& window, Image2D& render_image, vk::ImageView render_image_view) {
			Get().RenderImGuiAndPresentImpl(window, render_image, render_image_view);
		}


		inline static constexpr uint32_t READY_TO_REACQUIRE_SWAPCHAIN_IDX = std::numeric_limits<uint32_t>::max();
	private:
		static void PresentImage(Window& window, vk::Semaphore wait_semaphore);

		void RecordRenderDebugCommandsImpl(vk::CommandBuffer cmd_buf, Image2D& colour_image, Image2D& depth_image, DescriptorBuffer& scene_data_db);

		void InitImpl();

		void RenderImGuiAndPresentImpl(Window& window, Image2D& render_image, vk::ImageView render_image_view);

		vk::Semaphore AcquireNextSwapchainImageImpl(Window& window, uint32_t& image_index);

		uint32_t m_current_swapchain_image_index = READY_TO_REACQUIRE_SWAPCHAIN_IDX;

		std::array<vk::UniqueFence, MAX_FRAMES_IN_FLIGHT> m_in_flight_fences;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> imgui_cmd_buffers;
		std::array<vk::UniqueSemaphore, MAX_FRAMES_IN_FLIGHT> m_render_finished_semaphores;

		EventListener m_fence_sync_listener;
		std::vector<vk::UniqueSemaphore> m_image_avail_semaphores;

		GraphicsPipeline m_debug_pipeline;
		struct RenderData {
			std::vector<Line> lines;
			std::vector<Sphere> spheres;
		} m_render_data;
	};


}