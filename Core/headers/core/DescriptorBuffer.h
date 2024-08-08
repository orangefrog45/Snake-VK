#pragma once
#include "core/S_VkBuffer.h"
#include "textures/Textures.h"
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

		DescriptorSetSpec(DescriptorSetSpec&& other) :
			m_binding_offsets(std::move(other.m_binding_offsets)),
			m_layout_bindings(std::move(other.m_layout_bindings)), m_layout(std::move(other.m_layout)) {};

		DescriptorSetSpec& GenDescriptorLayout() {
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

		size_t GetSize() {
			return m_size;
		}

		bool IsBindingPointOccupied(unsigned binding) {
			return std::ranges::find_if(m_layout_bindings, [binding](const auto& p) {return p.binding == binding; }) != m_layout_bindings.end();
		}

		vk::DescriptorType GetDescriptorTypeAtBinding(unsigned binding) {
			auto it = std::ranges::find_if(m_layout_bindings, [binding](const auto& p) {return p.binding == binding; });
			SNK_ASSERT(it != m_layout_bindings.end());

			return it->descriptorType;
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

	class DescriptorBuffer {
	public:
		DescriptorBuffer() = default;
		~DescriptorBuffer() = default;
		DescriptorBuffer(const DescriptorBuffer& other) = delete;
		DescriptorBuffer& operator=(const DescriptorBuffer& other) = delete;

		DescriptorBuffer(DescriptorBuffer&& other) noexcept :
			mp_descriptor_spec(std::move(other.mp_descriptor_spec)),
			descriptor_buffer(std::move(other.descriptor_buffer)) {};

		void CreateBuffer(uint32_t num_sets) {
			SNK_ASSERT(mp_descriptor_spec);
			SNK_ASSERT(mp_descriptor_spec->m_layout_bindings.size() != 0);

			for (const auto& binding : mp_descriptor_spec->m_layout_bindings) {
				if (binding.descriptorType == vk::DescriptorType::eUniformBuffer)
					m_usage_flags |= vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT;
				else if (binding.descriptorType == vk::DescriptorType::eCombinedImageSampler)
					m_usage_flags |= vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT;
				else
					SNK_BREAK("Unsupported descriptorType in descriptor layout binding");
			}

			descriptor_buffer.CreateBuffer(mp_descriptor_spec->m_aligned_size * num_sets,
				m_usage_flags | vk::BufferUsageFlagBits::eShaderDeviceAddress, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		}

		void LinkResource(vk::DescriptorGetInfoEXT resource_info, unsigned binding_idx, unsigned set_buffer_idx) {
			auto& descriptor_buffer_properties = VulkanContext::GetPhysicalDevice().buffer_properties;

			size_t size = 0;
			auto descriptor_type = mp_descriptor_spec->GetDescriptorTypeAtBinding(binding_idx);
			if (descriptor_type == vk::DescriptorType::eUniformBuffer)
				size = descriptor_buffer_properties.uniformBufferDescriptorSize;
			else if (descriptor_type == vk::DescriptorType::eCombinedImageSampler)
				size = descriptor_buffer_properties.combinedImageSamplerDescriptorSize;
			else
				SNK_BREAK("LinkResource failed, unsupported descriptor type used");

			VulkanContext::GetLogicalDevice().device->getDescriptorEXT(resource_info, size,
				reinterpret_cast<std::byte*>(descriptor_buffer.Map()) + mp_descriptor_spec->GetBindingOffset(binding_idx) + mp_descriptor_spec->m_aligned_size * set_buffer_idx);
		}

		vk::DescriptorBufferBindingInfoEXT GetBindingInfo() {
			vk::DescriptorBufferBindingInfoEXT info{};
			info.address = descriptor_buffer.GetDeviceAddress();
			info.usage = m_usage_flags;
			return info;
		}

		// Returned pointer is alive as long as this object is alive
		DescriptorSetSpec* GetDescriptorSpec() {
			return mp_descriptor_spec.get();
		}

		void SetDescriptorSpec(std::shared_ptr<DescriptorSetSpec> spec) {
			mp_descriptor_spec = spec;
		}

		S_VkBuffer descriptor_buffer;

	private:
		std::shared_ptr<DescriptorSetSpec> mp_descriptor_spec = nullptr;

		vk::BufferUsageFlags m_usage_flags;
	};


	class MaterialAsset : public Asset {
	public:
		struct MaterialUpdateEvent : public Event {
			MaterialUpdateEvent(MaterialAsset* _mat) : p_material(_mat) { };
			MaterialAsset* p_material = nullptr;
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
		MaterialAsset(uint64_t uuid = 0) : Asset(uuid) {};

		inline static constexpr uint16_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint16_t>::max();

		uint16_t m_global_buffer_index = INVALID_GLOBAL_INDEX;

		friend class GlobalMaterialBufferManager;
		friend class AssetManager;
	};




	class GlobalMaterialBufferManager {
	public:
		void Init(const std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT>& buffers) {
			descriptor_buffers = buffers;
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
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
					reinterpret_cast<std::byte*>(descriptor_buffers[i]->descriptor_buffer.Map()) +
					descriptor_buffers[i]->GetDescriptorSpec()->GetBindingOffset(0));
			}

			m_material_update_event_listener.callback = [this](Event const* p_event) {
				auto const* p_casted = dynamic_cast<MaterialAsset::MaterialUpdateEvent const*>(p_event);
				
				for (uint8_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
					m_materials_to_update[i].push_back(p_casted->p_material);
				}
			};

			m_frame_start_listener.callback = [this](Event const* p_event) {
				auto* p_casted = dynamic_cast<FrameStartEvent const*>(p_event);
				UpdateMaterialUBO(p_casted->frame_flight_index);
				};

			EventManagerG::RegisterListener<MaterialAsset::MaterialUpdateEvent>(m_material_update_event_listener);
			EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
		}

		void RegisterMaterial(AssetRef<MaterialAsset> material) {
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_materials_to_register[i].push_back(material);
				m_materials_to_update[i].push_back(material);
			}
			material->m_global_buffer_index = m_current_index++;
		}

		// Size is doubled due to alignment requiring it, otherwise buffer sizes don't match on allocation
		// Will probably use up the space for something in the future anyway
		inline static constexpr uint32_t material_size = sizeof(uint32_t) * 8 * 2;

		std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT> descriptor_buffers{};
	private:
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> material_ubos;

		void UpdateMaterialUBO(FrameInFlightIndex frame_in_flight_idx) {
			std::byte* p_data = reinterpret_cast<std::byte*>(m_material_ubos[frame_in_flight_idx].Map());

			for (auto& mat_ref : m_materials_to_update[frame_in_flight_idx]) {
				SNK_ASSERT(mat_ref->GetGlobalBufferIndex() != MaterialAsset::INVALID_GLOBAL_INDEX, "");

				std::array<uint32_t, 5> texture_indices = { Texture2D::INVALID_GLOBAL_INDEX };

				if (mat_ref->p_albedo_tex) texture_indices[0] = mat_ref->p_albedo_tex->GetGlobalIndex();
				if (mat_ref->p_normal_tex) texture_indices[1] = mat_ref->p_normal_tex->GetGlobalIndex();
				if (mat_ref->p_roughness_tex) texture_indices[2] = mat_ref->p_roughness_tex->GetGlobalIndex();
				if (mat_ref->p_metallic_tex) texture_indices[3] = mat_ref->p_metallic_tex->GetGlobalIndex();
				if (mat_ref->p_ao_tex) texture_indices[4] = mat_ref->p_ao_tex->GetGlobalIndex();

				std::array<float, 3> params = { mat_ref->roughness, mat_ref->metallic, mat_ref->ao };

				memcpy(p_data + mat_ref->GetGlobalBufferIndex() * material_size, texture_indices.data(), texture_indices.size() * sizeof(uint32_t));
				memcpy(p_data + mat_ref->GetGlobalBufferIndex() * material_size + texture_indices.size() * sizeof(uint32_t), params.data(), params.size() * sizeof(float));
			}

			m_materials_to_update[frame_in_flight_idx].clear();
		}


		EventListener m_material_update_event_listener;
		EventListener m_frame_start_listener;
		// Current index to write to 
		uint16_t m_current_index = 0;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_material_ubos;

		std::unordered_map<FrameInFlightIndex, std::vector<AssetRef<MaterialAsset>>> m_materials_to_register;

		std::unordered_map<FrameInFlightIndex, std::vector<AssetRef<MaterialAsset>>> m_materials_to_update;
	};

	class GlobalTextureBufferManager {
	public:
		void Init(const std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT>& buffers) {
			descriptor_buffers = buffers;

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

		std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT> descriptor_buffers{};

	private:
		void RegisterTexturesInternal(FrameInFlightIndex frame_in_flight_idx) {
			for (auto* p_tex : m_textures_to_register[frame_in_flight_idx]) {
				auto& buffer_properties = VulkanContext::GetPhysicalDevice().buffer_properties;

				auto& spec = p_tex->GetSpec();
				vk::DescriptorImageInfo image_descriptor{};
				image_descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				image_descriptor.imageView = p_tex->GetImageView();
				image_descriptor.sampler = p_tex->GetSampler();

				// Use above image data to connect the descriptor at the offset provided to this specific image
				vk::DescriptorGetInfoEXT image_desc_info{};
				image_desc_info.type = vk::DescriptorType::eCombinedImageSampler;
				image_desc_info.data.pCombinedImageSampler = &image_descriptor;
				VulkanContext::GetLogicalDevice().device->getDescriptorEXT(image_desc_info,
					buffer_properties.combinedImageSamplerDescriptorSize,
					reinterpret_cast<std::byte*>(descriptor_buffers[frame_in_flight_idx]->descriptor_buffer.Map()) + p_tex->GetGlobalIndex() *
					buffer_properties.combinedImageSamplerDescriptorSize + descriptor_buffers[frame_in_flight_idx]->GetDescriptorSpec()->GetBindingOffset(1));

			}

			m_textures_to_register[frame_in_flight_idx].clear();
		}

		uint16_t m_current_index = 0;

		EventListener m_frame_start_listener;

		std::unordered_map<FrameInFlightIndex, std::vector<Texture2D*>> m_textures_to_register;
	};
}