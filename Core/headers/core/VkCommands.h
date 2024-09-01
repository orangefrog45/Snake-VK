#pragma once
#include "VkContext.h"
#include "util/util.h"

namespace SNAKE {
	struct CommandBuffer {
		void Init(vk::CommandBufferLevel level) {
			vk::CommandBufferAllocateInfo alloc_info{};
			alloc_info.commandPool = VkContext::GetCommandPool();
			alloc_info.level = level;
			alloc_info.commandBufferCount = 1;

			buf = std::move(VkContext::GetLogicalDevice().device->allocateCommandBuffersUnique(alloc_info).value[0]);
			SNK_ASSERT(buf);
		}

		vk::UniqueCommandBuffer buf;
	};
}