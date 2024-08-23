#include "pch/pch.h"
#include "core/DescriptorBuffer.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"

using namespace SNAKE;

void DescriptorBuffer::CreateBuffer(uint32_t num_sets) {
	SNK_ASSERT(mp_descriptor_spec);
	SNK_ASSERT(mp_descriptor_spec->m_layout_bindings.size() != 0);

	for (const auto& binding : mp_descriptor_spec->m_layout_bindings) {
		if (binding.descriptorType == vk::DescriptorType::eUniformBuffer || binding.descriptorType == vk::DescriptorType::eStorageBuffer)
			m_usage_flags |= vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT;
		else if (binding.descriptorType == vk::DescriptorType::eCombinedImageSampler)
			m_usage_flags |= vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT;
		else
			SNK_BREAK("Unsupported descriptorType in descriptor layout binding");
	}

	descriptor_buffer.CreateBuffer(mp_descriptor_spec->m_aligned_size * num_sets,
		m_usage_flags | vk::BufferUsageFlagBits::eShaderDeviceAddress, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
}

void DescriptorBuffer::LinkResource(vk::DescriptorGetInfoEXT* resource_info, unsigned binding_idx, unsigned set_buffer_idx, uint32_t array_idx) {
	auto& descriptor_buffer_properties = VulkanContext::GetPhysicalDevice().buffer_properties;

	size_t size = 0;
	auto descriptor_type = mp_descriptor_spec->GetDescriptorTypeAtBinding(binding_idx);

	switch (descriptor_type) {
	case vk::DescriptorType::eUniformBuffer:
		size = descriptor_buffer_properties.uniformBufferDescriptorSize;
		break;
	case vk::DescriptorType::eCombinedImageSampler:
		size = descriptor_buffer_properties.combinedImageSamplerDescriptorSize;
		break;
	case vk::DescriptorType::eStorageBuffer:
		size = descriptor_buffer_properties.storageBufferDescriptorSize;
		break;
	default:
		SNK_BREAK("LinkResource failed, unsupported descriptor type used");
		break;
	}

	VulkanContext::GetLogicalDevice().device->getDescriptorEXT(resource_info, size,
		reinterpret_cast<std::byte*>(descriptor_buffer.Map()) + mp_descriptor_spec->GetBindingOffset(binding_idx) + size * array_idx + mp_descriptor_spec->m_aligned_size * set_buffer_idx);
}

vk::DescriptorBufferBindingInfoEXT DescriptorBuffer::GetBindingInfo() {
	vk::DescriptorBufferBindingInfoEXT info{};
	info.address = descriptor_buffer.GetDeviceAddress();
	info.usage = m_usage_flags;
	return info;
}

// Returned pointer is alive as long as this object is alive
DescriptorSetSpec* DescriptorBuffer::GetDescriptorSpec() {
	return mp_descriptor_spec.get();
}

void DescriptorBuffer::SetDescriptorSpec(std::shared_ptr<DescriptorSetSpec> spec) {
	mp_descriptor_spec = spec;
}

DescriptorSetSpec& DescriptorSetSpec::GenDescriptorLayout() {
	vk::DescriptorSetLayoutCreateInfo layout_info{};
	layout_info.bindingCount = (uint32_t)m_layout_bindings.size();
	layout_info.pBindings = m_layout_bindings.data();
	layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT;
	auto [res, val] = VulkanContext::GetLogicalDevice().device->createDescriptorSetLayoutUnique(layout_info);
	SNK_CHECK_VK_RESULT(res);
	m_layout = std::move(val);

	auto& device = VulkanContext::GetLogicalDevice().device;

	for (uint32_t i = 0; i < m_layout_bindings.size(); i++) {
		m_binding_offsets[m_layout_bindings[i].binding] = device->getDescriptorSetLayoutBindingOffsetEXT(*m_layout, i);
	}

	m_size = device->getDescriptorSetLayoutSizeEXT(*m_layout);
	m_aligned_size = aligned_size(m_size, VulkanContext::GetPhysicalDevice().buffer_properties.descriptorBufferOffsetAlignment);

	return *this;
}

DescriptorSetSpec::BindingOffset DescriptorSetSpec::GetBindingOffset(BindingIndex binding_index) {
	SNK_ASSERT(m_binding_offsets.contains(binding_index));
	return m_binding_offsets[binding_index];
}

size_t DescriptorSetSpec::GetSize() {
	return m_size;
}

bool DescriptorSetSpec::IsBindingPointOccupied(unsigned binding) {
	return std::ranges::find_if(m_layout_bindings, [binding](const auto& p) {return p.binding == binding; }) != m_layout_bindings.end();
}

vk::DescriptorType DescriptorSetSpec::GetDescriptorTypeAtBinding(unsigned binding) {
	auto it = std::ranges::find_if(m_layout_bindings, [binding](const auto& p) {return p.binding == binding; });
	SNK_ASSERT(it != m_layout_bindings.end());

	return it->descriptorType;
}

DescriptorSetSpec& DescriptorSetSpec::AddDescriptor(unsigned binding_point, vk::DescriptorType type, vk::ShaderStageFlags flags, uint32_t descriptor_count) {
	vk::DescriptorSetLayoutBinding layout_binding{};
	layout_binding.binding = binding_point;
	layout_binding.descriptorType = type;
	layout_binding.descriptorCount = descriptor_count;
	layout_binding.stageFlags = flags;

	m_layout_bindings.push_back(layout_binding);

	return *this;
}

vk::DescriptorSetLayout DescriptorSetSpec::GetLayout() {
	return *m_layout;
}