#pragma once
#include "resources/S_VkBuffer.h"
#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "assets/Asset.h"
namespace SNAKE {

	class DescriptorSetSpec {
	public:
		DescriptorSetSpec() = default;
		~DescriptorSetSpec() = default;
		DescriptorSetSpec(const DescriptorSetSpec& other) = delete;
		DescriptorSetSpec& operator=(const DescriptorSetSpec& other) = delete;

		DescriptorSetSpec(DescriptorSetSpec&& other) noexcept :
			m_layout_bindings(std::move(other.m_layout_bindings)), m_layout(std::move(other.m_layout)),
			m_binding_offsets(std::move(other.m_binding_offsets)) {}

		DescriptorSetSpec& GenDescriptorLayout();

		size_t GetSize();

		bool IsBindingPointOccupied(unsigned binding) const;

		vk::DescriptorType GetDescriptorTypeAtBinding(unsigned binding);

		DescriptorSetSpec& AddDescriptor(unsigned binding_point, vk::DescriptorType type, vk::ShaderStageFlags flags, uint32_t descriptor_count = 1);

		vk::DescriptorSetLayout GetLayout() const;

		using BindingOffset = vk::DeviceAddress;
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

		void LinkResource(S_VkResource const* p_resource, DescriptorGetInfo& get_info, unsigned binding_idx, unsigned set_buffer_idx, uint32_t array_idx = 0 );

		vk::DescriptorBufferBindingInfoEXT GetBindingInfo();

		// Returned pointer is alive as long as this object is alive
		DescriptorSetSpec* GetDescriptorSpec();

		void SetDescriptorSpec(std::shared_ptr<DescriptorSetSpec> spec);

		S_VkBuffer descriptor_buffer;

	private:
		struct ResourceLinkInfo {
			DescriptorGetInfo get_info;
			std::shared_ptr<EventListener> p_resource_event_listener = nullptr;
			S_VkResource const* p_resource = nullptr;
		};

		std::shared_ptr<DescriptorSetSpec> mp_descriptor_spec = nullptr;
		std::vector<std::unordered_map<uint32_t, ResourceLinkInfo>> m_resource_link_infos;
		vk::BufferUsageFlags m_usage_flags;
	};

}