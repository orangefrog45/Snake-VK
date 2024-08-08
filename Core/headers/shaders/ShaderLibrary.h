#pragma once
#include "util/util.h"
#include "util/FileUtil.h"
#include "core/VkIncl.h"
#include "core/VkContext.h"
#include "spirv_cross.hpp"
#include "core/Pipelines.h"

namespace SNAKE {
	class ShaderLibrary {
	public:
		static vk::UniqueShaderModule CreateShaderModule(const std::string& filepath, PipelineLayoutBuilder& layout_builder) {
			using namespace spirv_cross;

			SNK_ASSERT(files::FileExists(filepath));
			auto code = files::ReadFileBinary(filepath);

			Compiler comp(reinterpret_cast<uint32_t*>(code.data()), code.size() / 4);
			
			auto res = comp.get_shader_resources();

			auto& descriptors = layout_builder.descriptor_set_layouts; // these are going out of scope and being destroyed, specs stored in builder?

			for (const Resource& resource : res.uniform_buffers) {
				unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
				unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);

				if (!descriptors.contains(set))
					descriptors[set] = std::make_shared<DescriptorSetSpec>();

				if (!descriptors[set]->IsBindingPointOccupied(binding))
					descriptors[set]->AddDescriptor(binding, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAllGraphics);
			}

			for (const Resource& resource : res.sampled_images) {
				unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
				unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
				auto type = comp.get_type(resource.type_id);

				bool is_array = type.array.size() != 0;
				unsigned descriptor_count = is_array ? type.array[0] : 1;

				if (!descriptors.contains(set))
					descriptors[set] = std::make_shared<DescriptorSetSpec>();

				if (!descriptors[set]->IsBindingPointOccupied(binding))
					descriptors[set]->AddDescriptor(binding, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAllGraphics, descriptor_count);
			}

			for (const Resource& resource : res.push_constant_buffers) {
				auto type = comp.get_type(resource.type_id);
				size_t size = comp.get_declared_struct_size(type);

				if (layout_builder.push_constants.empty())
					layout_builder.AddPushConstant(0, size, vk::ShaderStageFlagBits::eAllGraphics);
			}

			for (auto& [set_idx, set_spec] : descriptors) {
				set_spec->GenDescriptorLayout();
			}

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