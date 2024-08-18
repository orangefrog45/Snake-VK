#pragma once
#include "core/S_VkBuffer.h"
#include "textures/Textures.h"
#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "assets/Asset.h"
#include <iostream>
namespace SNAKE {

	class DescriptorSetSpec {
	public:
		DescriptorSetSpec() = default;
		~DescriptorSetSpec() = default;
		DescriptorSetSpec(const DescriptorSetSpec& other) = delete;
		DescriptorSetSpec& operator=(const DescriptorSetSpec& other) = delete;

		DescriptorSetSpec(DescriptorSetSpec&& other) :
			m_layout_bindings(std::move(other.m_layout_bindings)), m_layout(std::move(other.m_layout)),
			m_binding_offsets(std::move(other.m_binding_offsets)) {}

		DescriptorSetSpec& GenDescriptorLayout();

		size_t GetSize();

		bool IsBindingPointOccupied(unsigned binding);

		vk::DescriptorType GetDescriptorTypeAtBinding(unsigned binding);

		DescriptorSetSpec& AddDescriptor(unsigned binding_point, vk::DescriptorType type, vk::ShaderStageFlags flags, uint32_t descriptor_count = 1);

		vk::DescriptorSetLayout GetLayout();

		using BindingOffset = uint32_t;
		using BindingIndex = uint32_t;
		BindingOffset GetBindingOffset(BindingIndex binding_index);

	private:
		std::vector<vk::DescriptorSetLayoutBinding> m_layout_bindings;

		vk::UniqueDescriptorSetLayout m_layout;
		
		std::unordered_map<BindingIndex, BindingOffset> m_binding_offsets;

		size_t m_aligned_size = 0;
		size_t m_size = 0;

		friend class DescriptorBuffer;
	};

	class DescriptorBuffer {
	public:
		DescriptorBuffer() = default;
		~DescriptorBuffer() = default;
		DescriptorBuffer(const DescriptorBuffer& other) = delete;
		DescriptorBuffer& operator=(const DescriptorBuffer& other) = delete;

		DescriptorBuffer(DescriptorBuffer&& other) noexcept :
			descriptor_buffer(std::move(other.descriptor_buffer)),
			mp_descriptor_spec(std::move(other.mp_descriptor_spec)) {}

		void CreateBuffer(uint32_t num_sets);

		void LinkResource(vk::DescriptorGetInfoEXT resource_info, unsigned binding_idx, unsigned set_buffer_idx);

		vk::DescriptorBufferBindingInfoEXT GetBindingInfo();

		// Returned pointer is alive as long as this object is alive
		DescriptorSetSpec* GetDescriptorSpec();

		void SetDescriptorSpec(std::shared_ptr<DescriptorSetSpec> spec);

		S_VkBuffer descriptor_buffer;

	private:
		std::shared_ptr<DescriptorSetSpec> mp_descriptor_spec = nullptr;

		vk::BufferUsageFlags m_usage_flags;
	};

}