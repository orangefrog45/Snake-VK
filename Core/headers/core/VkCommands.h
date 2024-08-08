#pragma once
#include "VkIncl.h"
#include "util/util.h"

namespace SNAKE {
	struct CommandBuffer {
		void Init(vk::CommandPool pool) {
			vk::CommandBufferAllocateInfo alloc_info{};
			alloc_info.commandPool = pool;
			alloc_info.level = vk::CommandBufferLevel::ePrimary; // can be submitted to a queue for execution, can't be called by other command buffers
			alloc_info.commandBufferCount = 1;

			buf = std::move(VulkanContext::GetLogicalDevice().device->allocateCommandBuffersUnique(alloc_info).value[0]);
			SNK_ASSERT(buf, "");
		}

		vk::UniqueCommandBuffer buf;
	};
}