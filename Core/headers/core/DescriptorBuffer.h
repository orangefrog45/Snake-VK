#pragma once
#include "core/S_VkBuffer.h"
#include "textures/Textures.h"
#include "events/EventManager.h"
#include "events/EventsCommon.h"

namespace SNAKE {
	struct Descriptor {
		// Buffer or image
		void* p_resource = nullptr; 

		vk::DescriptorType type;

		// Binding index in shader, e.g (set = x, binding = binding_point)
		uint32_t binding_point; 

		// The set containing this descriptor
		struct DescriptorBufferSet* p_owner = nullptr;
	};

	struct DescriptorBufferSet {
		// Descriptors contained in the set
		std::vector<Descriptor> descriptors;

		// Index of this descriptor set from inside the descriptor buffer (p_owner)
		uint32_t set_index;

		// The buffer containing this set
		struct DescriptorBuffer* p_owner = nullptr;
	};

	class DescriptorSetSpec {
	public:
		DescriptorSetSpec() = default;
		~DescriptorSetSpec() = default;
		DescriptorSetSpec(const DescriptorSetSpec& other) = delete;
		DescriptorSetSpec& operator=(const DescriptorSetSpec& other) = delete;

		DescriptorSetSpec(DescriptorSetSpec&& other) :
			m_binding_offsets(std::move(other.m_binding_offsets)),
			m_layout_bindings(std::move(other.m_layout_bindings)), m_layout(std::move(other.m_layout)) {};

		void GenDescriptorLayout() {
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
		}

		size_t GetSize() {
			return m_size;
		}

		DescriptorSetSpec& AddDescriptor(unsigned binding_point, vk::DescriptorType type, vk::ShaderStageFlags flags, uint32_t descriptor_count = 1) {
			vk::DescriptorSetLayoutBinding layout_binding{};
			layout_binding.binding = binding_point;
			layout_binding.descriptorType = type;
			layout_binding.descriptorCount = descriptor_count;
			layout_binding.stageFlags = flags;

			m_layout_bindings.push_back(layout_binding);

			return *this;
		}

		vk::DescriptorSetLayout GetLayout() {
			return *m_layout;
		}

		using BindingOffset = uint32_t;
		using BindingIndex = uint32_t;
		BindingOffset GetBindingOffset(BindingIndex binding_index) {
			SNK_ASSERT(m_binding_offsets.contains(binding_index), "");
			return m_binding_offsets[binding_index];
		}

	private:
		std::vector<vk::DescriptorSetLayoutBinding> m_layout_bindings;

		vk::UniqueDescriptorSetLayout m_layout;
		
		std::unordered_map<BindingIndex, BindingOffset> m_binding_offsets;

		size_t m_aligned_size;
		size_t m_size;

		friend struct DescriptorBuffer;
	};

	struct DescriptorBuffer {
		DescriptorBuffer() = default;
		~DescriptorBuffer() = default;
		DescriptorBuffer(const DescriptorBuffer& other) = delete;
		DescriptorBuffer& operator=(const DescriptorBuffer& other) = delete;

		DescriptorBuffer(DescriptorBuffer&& other) noexcept :
			descriptor_spec(std::move(other.descriptor_spec)),
			descriptor_buffer(std::move(other.descriptor_buffer)) {};

		void CreateBuffer(uint32_t num_sets) {
			SNK_ASSERT(descriptor_spec.m_layout_bindings.size() != 0, "");

			vk::BufferUsageFlags flags;

			for (const auto& binding : descriptor_spec.m_layout_bindings) {
				if (binding.descriptorType == vk::DescriptorType::eUniformBuffer)
					flags |= vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT;
				else if (binding.descriptorType == vk::DescriptorType::eCombinedImageSampler)
					flags |= vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT;
				else
					SNK_BREAK("Unsupported descriptorType in descriptor layout binding");
			}

			descriptor_buffer.CreateBuffer(descriptor_spec.m_aligned_size * num_sets,
				flags | vk::BufferUsageFlagBits::eShaderDeviceAddress, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		}

		DescriptorSetSpec descriptor_spec;

		S_VkBuffer descriptor_buffer;
	};


	class Material {
	public:
		struct MaterialUpdateEvent : public Event {
			MaterialUpdateEvent(Material* _mat) : p_material(_mat) { };
			Material* p_material = nullptr;
		};

		uint16_t GetGlobalBufferIndex() {
			return m_global_buffer_index;
		}

		// Call after modifying any material data for this material to sequence updates for the gpu buffers
		void DispatchUpdateEvent() {
			EventManagerG::DispatchEvent(MaterialUpdateEvent(this));
		}

		Texture2D* p_albedo_tex = nullptr;
		Texture2D* p_normal_tex = nullptr;
		Texture2D* p_roughness_tex = nullptr;
		Texture2D* p_metallic_tex = nullptr;
		Texture2D* p_ao_tex = nullptr;

		float roughness = 0.5f;
		float metallic = 0.f;
		float ao = 0.2f;
	private:
		inline static constexpr uint16_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint16_t>::max();

		uint16_t m_global_buffer_index = INVALID_GLOBAL_INDEX;
		friend class GlobalMaterialDescriptorBuffer;
	};




	class GlobalMaterialDescriptorBuffer {
	public:
		void Init() {
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				descriptor_buffers[i].descriptor_spec.AddDescriptor(0, vk::DescriptorType::eUniformBuffer,
					vk::ShaderStageFlagBits::eAllGraphics, 64);

				descriptor_buffers[i].descriptor_spec.GenDescriptorLayout();

				descriptor_buffers[i].CreateBuffer(1);

				m_material_ubos[i].CreateBuffer(4096 * material_size,
					vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
					VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

				// Get address + size of uniform buffer
				vk::DescriptorAddressInfoEXT addr_info{};
				addr_info.address = m_material_ubos[i].GetDeviceAddress();
				addr_info.range = m_material_ubos[i].alloc_info.size;

				// Use above address + size data to connect the descriptor at the offset provided to this specific UBO
				vk::DescriptorGetInfoEXT buffer_descriptor_info{};
				buffer_descriptor_info.type = vk::DescriptorType::eUniformBuffer;
				buffer_descriptor_info.data.pUniformBuffer = &addr_info;

				VulkanContext::GetLogicalDevice().device->getDescriptorEXT(
					buffer_descriptor_info,
					VulkanContext::GetPhysicalDevice().buffer_properties.uniformBufferDescriptorSize,
					reinterpret_cast<std::byte*>(descriptor_buffers[i].descriptor_buffer.Map()) +
					descriptor_buffers[i].descriptor_spec.GetBindingOffset(0));
			}

			m_material_update_event_listener.callback = [this](Event const* p_event) {
				auto const* p_casted = dynamic_cast<Material::MaterialUpdateEvent const*>(p_event);
				
				for (uint8_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
					m_materials_to_update[i].push_back(p_casted->p_material);
				}
			};

			m_frame_start_listener.callback = [this](Event const* p_event) {
				auto* p_casted = dynamic_cast<FrameStartEvent const*>(p_event);
				UpdateMaterialUBO(p_casted->frame_flight_index);
				};

			EventManagerG::RegisterListener<Material::MaterialUpdateEvent>(m_material_update_event_listener);
			EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
		}

		void RegisterMaterial(Material& material) {
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_materials_to_register[i].push_back(&material);
				m_materials_to_update[i].push_back(&material);
			}
			material.m_global_buffer_index = m_current_index++;
		}

		// Size is doubled due to alignment requiring it, otherwise buffer sizes don't match on allocation
		// Will probably use up the space for something in the future anyway
		inline static constexpr uint32_t material_size = sizeof(uint32_t) * 8 * 2;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> descriptor_buffers{};

	private:
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> material_ubos;

		void UpdateMaterialUBO(FrameInFlightIndex frame_in_flight_idx) {
			std::byte* p_data = reinterpret_cast<std::byte*>(m_material_ubos[frame_in_flight_idx].Map());

			for (auto* p_mat : m_materials_to_update[frame_in_flight_idx]) {
				SNK_ASSERT(p_mat->GetGlobalBufferIndex() != Material::INVALID_GLOBAL_INDEX, "");

				std::array<uint32_t, 5> texture_indices = { Texture2D::INVALID_GLOBAL_INDEX };

				if (p_mat->p_albedo_tex) texture_indices[0] = p_mat->p_albedo_tex->GetGlobalIndex();
				if (p_mat->p_normal_tex) texture_indices[1] = p_mat->p_normal_tex->GetGlobalIndex();
				if (p_mat->p_roughness_tex) texture_indices[2] = p_mat->p_roughness_tex->GetGlobalIndex();
				if (p_mat->p_metallic_tex) texture_indices[3] = p_mat->p_metallic_tex->GetGlobalIndex();
				if (p_mat->p_ao_tex) texture_indices[4] = p_mat->p_ao_tex->GetGlobalIndex();

				std::array<float, 3> params = { p_mat->roughness, p_mat->metallic, p_mat->ao };

				memcpy(p_data + p_mat->GetGlobalBufferIndex() * material_size, texture_indices.data(), texture_indices.size() * sizeof(uint32_t));
				memcpy(p_data + p_mat->GetGlobalBufferIndex() * material_size + texture_indices.size() * sizeof(uint32_t), params.data(), params.size() * sizeof(float));
			}

			m_materials_to_update[frame_in_flight_idx].clear();
		}


		EventListener m_material_update_event_listener;
		EventListener m_frame_start_listener;
		// Current index to write to 
		uint16_t m_current_index = 0;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_material_ubos;

		std::unordered_map<FrameInFlightIndex, std::vector<Material*>> m_materials_to_register;

		std::unordered_map<FrameInFlightIndex, std::vector<Material*>> m_materials_to_update;
	};

	class GlobalTextureDescriptorBuffer {
	public:
		void Init() {
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				descriptor_buffers[i].descriptor_spec.AddDescriptor(0, vk::DescriptorType::eCombinedImageSampler,
					vk::ShaderStageFlagBits::eAllGraphics, 4096);

				descriptor_buffers[i].descriptor_spec.GenDescriptorLayout();

				descriptor_buffers[i].CreateBuffer(1);
			}

			m_frame_start_listener.callback = [this](Event const* p_event) {
				auto* p_casted = dynamic_cast<FrameStartEvent const*>(p_event);
				RegisterTexturesInternal(p_casted->frame_flight_index);
			};

			EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
		}

		void RegisterTexture(Texture2D& tex) {
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_textures_to_register[i].push_back(&tex);
			}
			tex.m_global_index = m_current_index++;
		}

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> descriptor_buffers{};

	private:
		void RegisterTexturesInternal(FrameInFlightIndex frame_in_flight_idx) {
			for (auto* p_tex : m_textures_to_register[frame_in_flight_idx]) {
				auto& buffer_properties = VulkanContext::GetPhysicalDevice().buffer_properties;

				auto& spec = p_tex->GetSpec();
				vk::DescriptorImageInfo image_descriptor{};
				image_descriptor.imageLayout = spec.current_layout;
				image_descriptor.imageView = p_tex->GetImageView();
				image_descriptor.sampler = p_tex->GetSampler();

				// Use above image data to connect the descriptor at the offset provided to this specific image
				vk::DescriptorGetInfoEXT image_desc_info{};
				image_desc_info.type = vk::DescriptorType::eCombinedImageSampler;
				image_desc_info.data.pCombinedImageSampler = &image_descriptor;
				VulkanContext::GetLogicalDevice().device->getDescriptorEXT(image_desc_info,
					buffer_properties.combinedImageSamplerDescriptorSize,
					reinterpret_cast<std::byte*>(descriptor_buffers[frame_in_flight_idx].descriptor_buffer.Map()) + p_tex->GetGlobalIndex() *
					buffer_properties.combinedImageSamplerDescriptorSize + descriptor_buffers[frame_in_flight_idx].descriptor_spec.GetBindingOffset(0));

			}

			m_textures_to_register[frame_in_flight_idx].clear();
		}

		uint16_t m_current_index = 0;

		EventListener m_frame_start_listener;

		std::unordered_map<FrameInFlightIndex, std::vector<Texture2D*>> m_textures_to_register;
	};
}