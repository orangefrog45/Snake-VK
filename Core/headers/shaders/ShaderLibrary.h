#pragma once
#include "util/util.h"
#include "util/FileUtil.h"
#include "core/VkIncl.h"
#include "core/VkContext.h"

namespace SNAKE {
	class ShaderLibrary {
	public:
		static vk::UniqueShaderModule CreateShaderModule(const std::string& filepath) {
			SNK_ASSERT(files::FileExists(filepath));
			auto code = files::ReadFileBinary(filepath);

			vk::ShaderModuleCreateInfo create_info{
				vk::ShaderModuleCreateFlags{0},
				code.size(), reinterpret_cast<const uint32_t*>(code.data())
			};

			vk::UniqueShaderModule shader_module = VulkanContext::GetLogicalDevice().device->createShaderModuleUnique(create_info).value;

			SNK_ASSERT(shader_module, "Shader module for shader '{0}' created successfully", filepath)
			return shader_module;
		}
	};
}