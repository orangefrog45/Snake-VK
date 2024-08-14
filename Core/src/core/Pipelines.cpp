#include "pch/pch.h"
#include "core/Pipelines.h"
#include "shaders/ShaderLibrary.h"

namespace SNAKE {

	void PipelineLayoutBuilder::Build() {
		if (!descriptor_set_layouts.empty()) {
			auto last = descriptor_set_layouts.end();
			last--;

			unsigned highest_idx = last->first;

			for (unsigned current_idx = 0; current_idx <= highest_idx; current_idx++) {
				if (descriptor_set_layouts.contains(current_idx))
					built_set_layouts.push_back(descriptor_set_layouts[current_idx]->GetLayout());
				else {
					descriptor_set_layouts[current_idx] = std::make_shared<DescriptorSetSpec>();
					descriptor_set_layouts[current_idx]->GenDescriptorLayout();
					built_set_layouts.push_back(descriptor_set_layouts[current_idx]->GetLayout());
				}
			}
		}

		pipeline_layout_info.setLayoutCount = built_set_layouts.size();
		pipeline_layout_info.pSetLayouts = built_set_layouts.data();
		pipeline_layout_info.pushConstantRangeCount = push_constants.size();
		pipeline_layout_info.pPushConstantRanges = push_constants.data();
	}

	GraphicsPipelineBuilder::GraphicsPipelineBuilder() {
		depth_stencil_info.depthTestEnable = true;
		depth_stencil_info.depthWriteEnable = true;
		depth_stencil_info.depthCompareOp = vk::CompareOp::eLessOrEqual;
		depth_stencil_info.depthBoundsTestEnable = false; // Only needed if keeping fragments that fall into a certain depth range
		depth_stencil_info.stencilTestEnable = false;

		rasterizer_info.depthClampEnable = VK_FALSE; // if fragments beyond near and far plane are clamped to them instead of discarding
		rasterizer_info.rasterizerDiscardEnable = VK_FALSE; // if geometry should never pass through rasterizer stage
		rasterizer_info.polygonMode = vk::PolygonMode::eFill;
		rasterizer_info.lineWidth = 1.f;
		rasterizer_info.cullMode = vk::CullModeFlagBits::eNone;
		rasterizer_info.frontFace = vk::FrontFace::eCounterClockwise;
		rasterizer_info.depthBiasEnable = VK_FALSE;

		dynamic_state_info.pDynamicStates = dynamic_states.data();
		dynamic_state_info.dynamicStateCount = dynamic_states.size();

		viewport_info.viewportCount = 1;
		viewport_info.scissorCount = 1;

		multisampling_info.sampleShadingEnable = VK_FALSE;
		multisampling_info.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling_info.minSampleShading = 1.f;

		input_assembly_info.topology = vk::PrimitiveTopology::eTriangleList;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::AddShader(vk::ShaderStageFlagBits stage, const std::string& filepath) {
		shader_modules.push_back(std::move(ShaderLibrary::CreateShaderModule(filepath, pipeline_layout_builder)));

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
		state.blendEnable = VK_TRUE;
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

		render_info.colorAttachmentCount = colour_attachment_formats.size();
		render_info.pColorAttachmentFormats = colour_attachment_formats.data();

		colour_blend_state.logicOpEnable = VK_FALSE;
		colour_blend_state.attachmentCount = colour_blend_attachment_states.size();
		colour_blend_state.pAttachments = colour_blend_attachment_states.data();

		vert_input_info.vertexBindingDescriptionCount = vertex_binding_descriptions.size();
		vert_input_info.vertexAttributeDescriptionCount = vertex_attribute_descriptions.size();
		vert_input_info.pVertexAttributeDescriptions = vertex_attribute_descriptions.data();
		vert_input_info.pVertexBindingDescriptions = vertex_binding_descriptions.data();

		pipeline_info.stageCount = shaders.size();
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

}