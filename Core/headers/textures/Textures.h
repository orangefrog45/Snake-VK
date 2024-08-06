#pragma once
#include "core/VkIncl.h"
#include "core/VkCommon.h"

namespace SNAKE {
	struct Image2DSpec {
		vk::Format format = vk::Format::eUndefined;
		vk::ImageUsageFlags usage;
		vk::ImageTiling tiling;
		glm::uvec2 size;
	};

	class Image2D {
	public:
		Image2D() = default;

		Image2D(const Image2DSpec& spec);

		Image2D(const Image2D& other) = delete;

		Image2D(Image2D&& other) = delete;

		Image2D& operator=(const Image2D& other) = delete;

		virtual ~Image2D() {
			DestroyImage();
		}

		void SetSpec(const Image2DSpec& spec) {
			m_spec = spec;
		}

		const Image2DSpec& GetSpec() {
			return m_spec;
		}

		void CreateImage(VmaAllocationCreateFlags flags=0);

		void LoadFromFile(const std::string& filepath, vk::CommandPool pool);

		void DestroyImage();

		void CreateImageView(vk::ImageAspectFlags aspect_flags);

		void CreateSampler();

		std::pair<vk::DescriptorGetInfoEXT, std::shared_ptr<vk::DescriptorImageInfo>> CreateDescriptorGetInfo(vk::ImageLayout layout) const;

		static void TransitionImageLayout(const vk::Image& image, vk::ImageAspectFlags aspect_flags, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandPool pool, vk::CommandBuffer buf = {});

		static void TransitionImageLayout(const vk::Image& image, vk::ImageAspectFlags aspect_flags, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandPool pool, 
			vk::AccessFlags src_access, vk::AccessFlags dst_access, vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage,  
			vk::CommandBuffer buf = {});

		vk::ImageView GetImageView() const {
			return *m_view;
		}

		vk::Sampler GetSampler() const {
			return *m_sampler;
		}

		vk::Image GetImage() const noexcept {
			return m_image;
		}

	private:
		Image2DSpec m_spec;

		vk::UniqueSampler m_sampler;
		vk::Image m_image;
		vk::UniqueImageView m_view;
		VmaAllocation m_allocation;
	};

	class Texture2D : public Image2D {
	public:
		inline uint16_t GetGlobalIndex() const { return m_global_index; };

		inline static constexpr uint16_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint16_t>::max();
	private:

		// Index into the global texture descriptor buffer
		uint16_t m_global_index = INVALID_GLOBAL_INDEX;

		friend class GlobalTextureDescriptorBuffer;
	};
}