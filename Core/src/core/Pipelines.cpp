#include "pch/pch.h"
#include "core/Pipelines.h"
#include "shaders/ShaderLibrary.h"

namespace SNAKE {

	void PipelineLayoutBuilder::Build() {
		static std::shared_ptr<DescriptorSetSpec> null_spec = nullptr;
		if (!null_spec) {
			null_spec = std::make_shared<DescriptorSetSpec>();
			null_spec->GenDescriptorLayout();
		}

		if (!descriptor_set_layouts.empty()) {
			auto last = descriptor_set_layouts.end();
			last--;

			unsigned highest_idx = last->first;

			for (unsigned current_idx = 0; current_idx <= highest_idx; current_idx++) {
				if (descriptor_set_layouts.contains(current_idx)) {
					built_set_layouts.push_back(descriptor_set_layouts[current_idx].lock()->GetLayout());
				}
				else {
					descriptor_set_layouts[current_idx] = null_spec;
					built_set_layouts.push_back(null_spec->GetLayout());
				}
			}
		}

		pipeline_layout_info.setLayoutCount = (uint32_t)built_set_layouts.size();
		pipeline_layout_info.pSetLayouts = built_set_layouts.data();
		pipeline_layout_info.pushConstantRangeCount = (uint32_t)push_constants.size();
		pipeline_layout_info.pPushConstantRanges = push_constants.data();
	}

	GraphicsPipelineBuilder::GraphicsPipelineBuilder() {
		depth_stencil_info.depthTestEnable = true;
		depth_stencil_info.depthWriteEnable = true;
		depth_stencil_info.depthCompareOp = vk::CompareOp::eLess;
		depth_stencil_info.depthBoundsTestEnable = false; // Only needed if keeping fragments that fall into a certain depth range
		depth_stencil_info.stencilTestEnable = false;

		rasterizer_info.depthClampEnable = VK_FALSE; // if fragments beyond near and far plane are clamped to them instead of discarding
		rasterizer_info.rasterizerDiscardEnable = VK_FALSE; // if geometry should never pass through rasterizer stage
		rasterizer_info.polygonMode = vk::PolygonMode::eFill;
		rasterizer_info.lineWidth = 1.f;
		rasterizer_info.cullMode = vk::CullModeFlagBits::eBack;
		rasterizer_info.frontFace = vk::FrontFace::eCounterClockwise;
		rasterizer_info.depthBiasEnable = VK_FALSE;

		dynamic_state_info.pDynamicStates = dynamic_states.data();
		dynamic_state_info.dynamicStateCount = (uint32_t)dynamic_states.size();

		viewport_info.viewportCount = 1;
		viewport_info.scissorCount = 1;

		multisampling_info.sampleShadingEnable = VK_FALSE;
		multisampling_info.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling_info.minSampleShading = 1.f;

		input_assembly_info.topology = vk::PrimitiveTopology::eTriangleList;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddShader(vk::ShaderStageFlagBits stage, const std::string& filepath) {
		shader_modules.push_back(ShaderLibrary::CreateShaderModule(filepath, pipeline_layout_builder));

		auto& module = shader_modules[shader_modules.size() - 1];

		vk::PipelineShaderStageCreateInfo info;
		info.module = *module;
		info.stage = stage;
		info.pName = "main";

		shaders.push_back(info);
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddColourAttachment(vk::Format format) {
		colour_attachment_formats.push_back(format);

		vk::PipelineColorBlendAttachmentState state;
		state.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		state.blendEnable = VK_FALSE;
		state.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
		state.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		state.colorBlendOp = vk::BlendOp::eAdd;
		state.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		state.dstAlphaBlendFactor = vk::BlendFactor::eZero;
		state.alphaBlendOp = vk::BlendOp::eAdd;

		colour_blend_attachment_states.push_back(state);
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddDepthAttachment(vk::Format format) {
		render_info.depthAttachmentFormat = format;
		return *this;
	}


	void GraphicsPipelineBuilder::Build() {
		pipeline_layout_builder.Build();
		pipeline_layout.Init(pipeline_layout_builder);

		render_info.colorAttachmentCount = (uint32_t)colour_attachment_formats.size();
		render_info.pColorAttachmentFormats = colour_attachment_formats.data();

		colour_blend_state.logicOpEnable = VK_FALSE;
		colour_blend_state.attachmentCount = (uint32_t)colour_blend_attachment_states.size();
		colour_blend_state.pAttachments = colour_blend_attachment_states.data();

		vert_input_info.vertexBindingDescriptionCount = (uint32_t)vertex_binding_descriptions.size();
		vert_input_info.vertexAttributeDescriptionCount = (uint32_t)vertex_attribute_descriptions.size();
		vert_input_info.pVertexAttributeDescriptions = vertex_attribute_descriptions.data();
		vert_input_info.pVertexBindingDescriptions = vertex_binding_descriptions.data();

		pipeline_info.stageCount = (uint32_t)shaders.size();
		pipeline_info.pStages = shaders.data();
		pipeline_info.pVertexInputState = &vert_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly_info;
		pipeline_info.pViewportState = &viewport_info;
		pipeline_info.pRasterizationState = &rasterizer_info;
		pipeline_info.pMultisampleState = &multisampling_info;
		pipeline_info.pDepthStencilState = &depth_stencil_info;
		pipeline_info.pColorBlendState = &colour_blend_state;
		pipeline_info.pDynamicState = &dynamic_state_info;
		pipeline_info.layout = pipeline_layout.GetPipelineLayout();
		pipeline_info.renderPass = nullptr;
		pipeline_info.subpass = 0; // index of subpass where this pipeline is used
		pipeline_info.pNext = &render_info;
		pipeline_info.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT;
	}

	RtPipelineBuilder& RtPipelineBuilder::AddShader(vk::ShaderStageFlagBits shader_stage, const std::string& filepath) {
		shader_modules.push_back(ShaderLibrary::CreateShaderModule(filepath));

		vk::PipelineShaderStageCreateInfo shader_stage_info{};
		shader_stage_info.stage = shader_stage;
		shader_stage_info.module = *shader_modules[shader_modules.size() - 1];
		shader_stage_info.pName = "main";

		shaders.push_back(shader_stage_info);
		shader_counts[shader_stage]++;
		return *this;
	}

	RtPipelineBuilder& RtPipelineBuilder::AddShaderGroup(vk::RayTracingShaderGroupTypeKHR group, uint32_t general_shader, uint32_t chit_shader,
		uint32_t any_hit_shader, uint32_t intersection_shader) {

		vk::RayTracingShaderGroupCreateInfoKHR group_info{};
		group_info.type = group;
		group_info.generalShader = general_shader;
		group_info.closestHitShader = chit_shader;
		group_info.anyHitShader = any_hit_shader;
		group_info.intersectionShader = intersection_shader;

		shader_groups.push_back(group_info);
		return *this;
	}

	void RtPipeline::Init(RtPipelineBuilder& builder, PipelineLayoutBuilder& layout) {
		m_layout.Init(layout);
		
		vk::RayTracingPipelineCreateInfoKHR pipeline_info{};
		pipeline_info.stageCount = (uint32_t)builder.shaders.size();
		pipeline_info.pStages = builder.shaders.data();
		pipeline_info.groupCount = (uint32_t)builder.shader_groups.size();
		pipeline_info.pGroups = builder.shader_groups.data();
		pipeline_info.maxPipelineRayRecursionDepth = 3;
		pipeline_info.layout = m_layout.GetPipelineLayout();
		pipeline_info.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT;

		auto [res, val] = VkContext::GetLogicalDevice().device->createRayTracingPipelineKHRUnique(nullptr, nullptr, pipeline_info);
		SNK_CHECK_VK_RESULT(res);
		m_pipeline = std::move(val);

		m_sbt.Init(*m_pipeline, builder);
	}


	void ComputePipeline::Init(const std::string& shader_path) {
		PipelineLayoutBuilder layout_builder{};

		auto shader_module = ShaderLibrary::CreateShaderModule(shader_path, layout_builder);
		layout_builder.Build();

		pipeline_layout.Init(layout_builder);

		vk::PipelineShaderStageCreateInfo stage_info{};
		stage_info.pName = "main";
		stage_info.module = *shader_module;
		stage_info.stage = vk::ShaderStageFlagBits::eCompute;

		vk::ComputePipelineCreateInfo pipeline_info{};
		pipeline_info.layout = pipeline_layout.GetPipelineLayout();
		pipeline_info.stage = stage_info;
		pipeline_info.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT;

		auto [res, pipeline] = VkContext::GetLogicalDevice().device->createComputePipelineUnique(VK_NULL_HANDLE, pipeline_info);
		SNK_CHECK_VK_RESULT(res);
		m_pipeline = std::move(pipeline);
	}


}