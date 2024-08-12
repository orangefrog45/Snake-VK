#pragma once
#include "core/Window.h"
#include "util/util.h"

namespace SNAKE {
	struct SwapchainInvalidateEvent : public Event {
		SwapchainInvalidateEvent(glm::vec2 new_extents) : new_swapchain_extents(new_extents) {};
		glm::vec2 new_swapchain_extents{ 0, 0 };
	};

	class VkRenderer {
	public:
		static VkRenderer& Get() {
			static VkRenderer instance;
			return instance;
		}

		static void Init() {
			vk::SemaphoreCreateInfo semaphore_info{};

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				Get().m_image_avail_semaphores.push_back(std::move(VulkanContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value));
			}

			SNK_ASSERT(Get().m_image_avail_semaphores[0]);
		}


		static vk::Semaphore AcquireNextSwapchainImage(Window& window, uint32_t& image_index) {
			SNK_CHECK_VK_RESULT(VulkanContext::GetLogicalDevice().device->acquireNextImageKHR(*window.GetVkContext().swapchain, UINT64_MAX, 
				*Get().m_image_avail_semaphores[VulkanContext::GetCurrentFIF()], VK_NULL_HANDLE, &image_index));

			return *Get().m_image_avail_semaphores[VulkanContext::GetCurrentFIF()];
		}

		static void PresentImage(Window& window, uint32_t num_semaphores, vk::Semaphore* signal_semaphores, uint32_t image_index) {
			auto swapchains = util::array(*window.GetVkContext().swapchain);

			vk::PresentInfoKHR present_info{};
			present_info.waitSemaphoreCount = num_semaphores;
			present_info.pWaitSemaphores = signal_semaphores;
			present_info.swapchainCount = 1;
			present_info.pSwapchains = swapchains.data();
			present_info.pImageIndices = &image_index;

			// Queue presentation work that waits on the render_finished semaphore
			auto present_result = VulkanContext::GetLogicalDevice().presentation_queue.presentKHR(present_info);
			if (present_result == vk::Result::eErrorOutOfDateKHR || window.WasJustResized() || present_result == vk::Result::eSuboptimalKHR) {
				window.OnPresentNeedsResize();
				EventManagerG::DispatchEvent(SwapchainInvalidateEvent{ {window.GetWidth(), window.GetHeight()} });
			}
			else if (present_result != vk::Result::eSuccess) {
				SNK_BREAK("Failed to present image");
			}
		}

	private:
		std::vector<vk::UniqueSemaphore> m_image_avail_semaphores;
	};


}