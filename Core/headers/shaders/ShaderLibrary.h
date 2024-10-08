#pragma once
#include <spirv_cross.hpp>

#include "core/VkIncl.h"
#include "core/VkContext.h"
#include "core/Pipelines.h"
#include "util/util.h"
#include "util/FileUtil.h"


namespace SNAKE {
	class ShaderLibrary {
	public:
		// Creates shader module from spv file at "filepath"
		// Optional layout_builder will have shader descriptors/push constants reflected into it to automate pipeline layout creation
		static vk::UniqueShaderModule CreateShaderModule(const std::string& filepath, std::optional<std::reference_wrapper<PipelineLayoutBuilder>> layout_builder = std::nullopt) {
			using namespace spirv_cross;

			SNK_ASSERT(files::PathExists(filepath));
			std::vector<std::byte> output;

			if (!files::ReadBinaryFile(filepath, output)) {
				SNK_BREAK("CreateShaderModule failed, failed to read file '{}'", filepath);
			}

			vk::ShaderModuleCreateInfo create_info{
				vk::ShaderModuleCreateFlags{0},
				output.size(), reinterpret_cast<const uint32_t*>(output.data())
			};

			vk::UniqueShaderModule shader_module = VkContext::GetLogicalDevice().device->createShaderModuleUnique(create_info).value;
			SNK_ASSERT(shader_module);

			if (!layout_builder.has_value())
				return shader_module;


			Compiler comp(reinterpret_cast<uint32_t*>(output.data()), output.size() / 4);
			
			auto res = comp.get_shader_resources();

			auto& builder = layout_builder.value().get();
			auto& descriptors = builder.descriptor_set_layouts;
			auto& spec_output = builder.reflected_descriptor_specs;
			std::unordered_map<uint32_t, std::shared_ptr<DescriptorSetSpec>> new_specs;

			// Allocate space so pushing back elements doesn't cause vector reallocation (breaks pointers)
			constexpr unsigned MAX_DESCRIPTOR_SETS = 16;
			spec_output.reserve(MAX_DESCRIPTOR_SETS);
			for (const Resource& resource : res.uniform_buffers) {
				unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
				unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);

				if (!new_specs.contains(set)) {
					auto spec = std::make_shared<DescriptorSetSpec>();
					spec_output.push_back(spec);
					new_specs[set] = spec;
					descriptors[set] = spec;
				}

				if (!new_specs[set]->IsBindingPointOccupied(binding))
					new_specs[set]->AddDescriptor(binding, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll);
			}

			for (const Resource& resource : res.sampled_images) {
				unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
				unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
				auto type = comp.get_type(resource.type_id);

				bool is_array = type.array.size() != 0;
				unsigned descriptor_count = is_array ? type.array[0] : 1;

				if (!new_specs.contains(set)) {
					auto spec = std::make_shared<DescriptorSetSpec>();
					spec_output.push_back(spec);
					new_specs[set] = spec;
					descriptors[set] = spec;
				}

				if (!new_specs[set]->IsBindingPointOccupied(binding))
					new_specs[set]->AddDescriptor(binding, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAll, descriptor_count);
			}

			for (const Resource& resource : res.storage_images) {
				unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
				unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
				auto type = comp.get_type(resource.type_id);

				bool is_array = type.array.size() != 0;
				unsigned descriptor_count = is_array ? type.array[0] : 1;

				if (!new_specs.contains(set)) {
					auto spec = std::make_shared<DescriptorSetSpec>();
					spec_output.push_back(spec);
					new_specs[set] = spec;
					descriptors[set] = spec;
				}

				if (!new_specs[set]->IsBindingPointOccupied(binding))
					new_specs[set]->AddDescriptor(binding, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eAll, descriptor_count);
			}

			for (const Resource& resource : res.storage_buffers) {
				unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
				unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
				auto type = comp.get_type(resource.type_id);

				if (!new_specs.contains(set)) {
					auto spec = std::make_shared<DescriptorSetSpec>();
					spec_output.push_back(spec);
					new_specs[set] = spec;
					descriptors[set] = spec;
				}

				if (!new_specs[set]->IsBindingPointOccupied(binding))
					new_specs[set]->AddDescriptor(binding, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll);
			}

			for (const Resource& resource : res.push_constant_buffers) {
				auto type = comp.get_type(resource.type_id);
				size_t size = comp.get_declared_struct_size(type);

				if (builder.push_constants.empty())
					builder.AddPushConstant(0u, (uint32_t)size, vk::ShaderStageFlagBits::eAll);
			}

			SNK_ASSERT(spec_output.size() < MAX_DESCRIPTOR_SETS);

			for (auto& [set_idx, set_spec] : new_specs) {
				set_spec->GenDescriptorLayout();
			}


			return shader_module;
		}
	};
}