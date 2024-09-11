#pragma once
#include "VkIncl.h"
#include "util/util.h"
#include "VkContext.h"
#include "core/DescriptorBuffer.h"

namespace SNAKE {
	struct PipelineLayoutBuilder {
		void Build();

		PipelineLayoutBuilder& AddDescriptorSet(uint32_t set_idx, const DescriptorSetSpec& spec) {
			descriptor_set_layouts[set_idx] = &spec;
			return *this;
		}

		PipelineLayoutBuilder& AddPushConstant(uint32_t offset, uint32_t size, vk::ShaderStageFlagBits stage_flags) {
			push_constants.push_back(vk::PushConstantRange{ stage_flags, offset, size });
			return *this;
		}

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		std::vector<vk::PushConstantRange> push_constants;
		std::map<uint32_t, DescriptorSetSpec const*> descriptor_set_layouts;
		std::vector<DescriptorSetSpec> reflected_descriptor_specs;
		std::vector<vk::DescriptorSetLayout> built_set_layouts;
	};

	class PipelineLayout {
	public:
		PipelineLayout() = default;
		~PipelineLayout() = default;
		PipelineLayout(const PipelineLayout& other) = delete;
		PipelineLayout(PipelineLayout&& other) noexcept : pipeline_layout(std::move(other.pipeline_layout)) {};
		PipelineLayout& operator=(const PipelineLayout& other) = delete;

		void Init(PipelineLayoutBuilder& builder) {
			SNK_ASSERT(!pipeline_layout);
			auto [res, val] = VkContext::GetLogicalDevice().device->createPipelineLayoutUnique(builder.pipeline_layout_info);
			SNK_CHECK_VK_RESULT(res);
			pipeline_layout = std::move(val);

			descriptor_set_layouts = std::move(builder.descriptor_set_layouts);
			reflected_descriptor_specs = std::move(builder.reflected_descriptor_specs);
		}

		vk::PipelineLayout GetPipelineLayout() {
			return *pipeline_layout;
		}
	private:
		std::map<uint32_t, DescriptorSetSpec const*> descriptor_set_layouts;
		std::vector<DescriptorSetSpec> reflected_descriptor_specs;
		vk::UniquePipelineLayout pipeline_layout;

		friend class GraphicsPipeline;
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

		void Build();

		PipelineLayoutBuilder pipeline_layout_builder;
		PipelineLayout pipeline_layout;

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

		void Init( GraphicsPipelineBuilder& builder) {
			SNK_ASSERT(!m_pipeline);
			auto [res, pipeline] = VkContext::GetLogicalDevice().device->createGraphicsPipelineUnique(VK_NULL_HANDLE, builder.pipeline_info);
			SNK_CHECK_VK_RESULT(res);
			m_pipeline = std::move(pipeline);
			pipeline_layout.pipeline_layout = std::move(builder.pipeline_layout.pipeline_layout);
			pipeline_layout.descriptor_set_layouts = std::move(builder.pipeline_layout.descriptor_set_layouts);
		}

		vk::Pipeline GetPipeline() {
			return *m_pipeline;
		}

		PipelineLayout pipeline_layout;
	private:
		vk::UniquePipeline m_pipeline;
	};

	struct RtPipelineBuilder {
		RtPipelineBuilder& AddShader(vk::ShaderStageFlagBits shader_stage, const std::string& filepath);
		RtPipelineBuilder& AddShaderGroup(vk::RayTracingShaderGroupTypeKHR group, uint32_t general_shader, uint32_t chit_shader, uint32_t any_hit_shader, uint32_t intersection_shader);

		std::vector<vk::PipelineShaderStageCreateInfo> shaders;
		std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_groups;
		std::vector<vk::UniqueShaderModule> shader_modules;
	};

	class RtPipeline {
	public:
		void Init(RtPipelineBuilder& builder, PipelineLayoutBuilder& layout);

		vk::Pipeline GetPipeline() {
			return *m_pipeline;
		}

		vk::PipelineLayout GetPipelineLayout() {
			return m_layout.GetPipelineLayout();
		}
	private:
		PipelineLayout m_layout;
		vk::UniquePipeline m_pipeline;
	};
}