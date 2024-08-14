#pragma once
#include "core/VkIncl.h"
#include "core/VkCommon.h"
#include "events/EventsCommon.h"

namespace SNAKE {
	struct Image2DSpec {
		vk::Format format = vk::Format::eUndefined;
		vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
		vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
		vk::ImageAspectFlags aspect_flags = vk::ImageAspectFlagBits::eNone;
		glm::uvec2 size{ 0, 0 };
	};

	class Image2D {
	public:
		Image2D() = default;

		// Creates an Image2D "wrapper" around an existing vkImage, this class will not handle deallocation if using this constructor
		Image2D(vk::Image image, Image2DSpec spec) : m_image(image), m_owns_image(false) {
			SetSpec(spec);
		}

		Image2D(const Image2DSpec& spec);

		Image2D(const Image2D& other) = delete;

		Image2D(Image2D&& other) = delete;

		Image2D& operator=(const Image2D& other) = delete;

		virtual ~Image2D() {
			if (m_owns_image)
				DestroyImage();
		}

		void SetSpec(const Image2DSpec& spec) {
			m_spec = spec;
		}

		const Image2DSpec& GetSpec() {
			return m_spec;
		}

		void CreateImage(VmaAllocationCreateFlags flags=0);

		void DestroyImage();

		void CreateImageView();

		void DestroyImageView() {
			m_view.release();
		}

		void DestroySampler() {
			m_sampler.release();
		}

		void CreateSampler();

		std::pair<vk::DescriptorGetInfoEXT, std::shared_ptr<vk::DescriptorImageInfo>> CreateDescriptorGetInfo(vk::ImageLayout layout) const;

		void TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandBuffer buf = {});

		void TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout, 
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

	protected:
		Image2DSpec m_spec;

		vk::UniqueSampler m_sampler;
		vk::Image m_image;
		vk::UniqueImageView m_view;
		VmaAllocation m_allocation;

		const bool m_owns_image = true;
	};

	// An image that always resizes itself to the current active window dimensions
	class FullscreenImage2D : public Image2D {
	public:
		FullscreenImage2D();
	private:
		EventListener m_swapchain_invalidate_listener;
	};

	class Texture2D : public Image2D {
	public:
		inline uint16_t GetGlobalIndex() const { return m_global_index; };

		void LoadFromFile(const std::string& filepath);

		inline static constexpr uint16_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint16_t>::max();
	private:

		// Index into the global texture descriptor buffer
		uint16_t m_global_index = INVALID_GLOBAL_INDEX;

		friend class GlobalTextureBufferManager;
	};
}