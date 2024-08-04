#pragma once
#include "VkIncl.h"
#include "util/util.h"
#include "VkContext.h"

namespace SNAKE {
	struct PipelineLayoutBuilder {
		void Build() {
			pipeline_layout_info.setLayoutCount = descriptor_set_layouts.size();
			pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
			pipeline_layout_info.pushConstantRangeCount = push_constants.size();
			pipeline_layout_info.pPushConstantRanges = push_constants.data();
		}

		PipelineLayoutBuilder& SetDescriptorSetLayouts(const std::vector<vk::DescriptorSetLayout>& layouts) {
			descriptor_set_layouts = layouts;
			return *this;
		}

		PipelineLayoutBuilder& AddPushConstant(uint32_t offset, uint32_t size, vk::ShaderStageFlagBits stage_flags) {
			push_constants.push_back(vk::PushConstantRange{ stage_flags, offset, size });
			return *this;
		}

		vk::PipelineLayoutCreateInfo pipeline_layout_info;

		std::vector<vk::PushConstantRange> push_constants;
		std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
	};

	class PipelineLayout {
	public:
		PipelineLayout() = default;
		~PipelineLayout() = default;
		PipelineLayout(const PipelineLayout& other) = delete;
		PipelineLayout(PipelineLayout&& other) = delete; 
		PipelineLayout& operator=(const PipelineLayout& other) = delete;

		void Init(const PipelineLayoutBuilder& builder) {
			SNK_ASSERT(!pipeline_layout);
			pipeline_layout = VulkanContext::GetLogicalDevice().device->createPipelineLayoutUnique(builder.pipeline_layout_info).value;
		}

		vk::PipelineLayout GetPipelineLayout() {
			return *pipeline_layout;
		}

	private:
		vk::UniquePipelineLayout pipeline_layout;
	};

	struct GraphicsPipelineBuilder {
		GraphicsPipelineBuilder();

		GraphicsPipelineBuilder& AddShader(vk::ShaderStageFlagBits stage, const std::string& filepath);

		GraphicsPipelineBuilder& AddColourAttachment(vk::Format format);

		GraphicsPipelineBuilder& AddDepthAttachment(vk::Format format);

		GraphicsPipelineBuilder& AddVertexBinding(const vk::VertexInputAttributeDescription& attribute_desc, const vk::VertexInputBindingDescription& binding_desc) {
			vertex_attribute_descriptions.push_back(attribute_desc);
			vertex_binding_descriptions.push_back(binding_desc);
			return *this;
		}

		GraphicsPipelineBuilder& SetPipelineLayout(vk::PipelineLayout _layout) {
			pipeline_layout = _layout;
			return *this;
		}

		void Build();

		vk::PipelineLayout pipeline_layout;

		vk::GraphicsPipelineCreateInfo pipeline_info;

		std::vector<vk::PipelineShaderStageCreateInfo> shaders;
		std::vector<vk::UniqueShaderModule> shader_modules;

		std::vector<vk::VertexInputAttributeDescription> vertex_attribute_descriptions;
		std::vector<vk::VertexInputBindingDescription> vertex_binding_descriptions;
		vk::PipelineVertexInputStateCreateInfo vert_input_info;

		vk::PipelineInputAssemblyStateCreateInfo input_assembly_info;
		vk::PipelineDynamicStateCreateInfo dynamic_state_info;
		vk::PipelineViewportStateCreateInfo viewport_info;
		vk::PipelineRasterizationStateCreateInfo rasterizer_info;
		vk::PipelineMultisampleStateCreateInfo multisampling_info;

		std::vector<vk::PipelineColorBlendAttachmentState> colour_blend_attachment_states;
		std::vector<vk::Format> colour_attachment_formats;

		vk::PipelineRenderingCreateInfo render_info;
		vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;

		//AUTO
		vk::PipelineColorBlendStateCreateInfo colour_blend_state;

		std::array<vk::DynamicState, 2> dynamic_states = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
	};

	class GraphicsPipeline {
	public:
		GraphicsPipeline() = default;
		~GraphicsPipeline() = default;
		GraphicsPipeline(const GraphicsPipeline& other) = delete;
		GraphicsPipeline(GraphicsPipeline&& other) = delete;
		GraphicsPipeline& operator=(const GraphicsPipeline& other) = delete;

		void Init(const GraphicsPipelineBuilder& builder) {
			SNK_ASSERT(!m_pipeline);
			auto [res, pipeline] = VulkanContext::GetLogicalDevice().device->createGraphicsPipelineUnique(VK_NULL_HANDLE, builder.pipeline_info);
			SNK_CHECK_VK_RESULT(res);
			m_pipeline = std::move(pipeline);
		}

		vk::Pipeline GetPipeline() {
			return *m_pipeline;
		}

	private:
		vk::UniquePipeline m_pipeline;
	};
}