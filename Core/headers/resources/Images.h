#pragma once
#include "core/VkIncl.h"
#include "core/VkCommon.h"
#include "resources/S_VkResource.h"
#include "events/EventsCommon.h"

namespace SNAKE {
	struct Image2DSpec {
		Image2DSpec() = default;
		Image2DSpec(vk::Format fmt, vk::ImageUsageFlags use_flags, vk::ImageTiling _tiling, vk::ImageAspectFlags asp_flags, glm::uvec2 _size, float mips) :
			format(fmt), usage(use_flags), tiling(_tiling), aspect_flags(asp_flags), size(_size), mip_levels(mips) {}

		vk::Format format = vk::Format::eUndefined;
		vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
		vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
		vk::ImageAspectFlags aspect_flags = vk::ImageAspectFlagBits::eNone;
		unsigned mip_levels = 1;
		glm::uvec2 size{ 0, 0 };
	};

	class Image2D : public S_VkResource {
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

		void GenerateMipmaps(vk::ImageLayout start_layout);

		void RefreshDescriptorGetInfo(DescriptorGetInfo& info) const override;

		// Returns an image view created with format 'fmt' (if provided) or (if not provided) image.m_spec.format
		[[nodiscard]] static vk::UniqueImageView CreateImageView(const Image2D& image, std::optional<vk::Format> fmt = std::nullopt);

		void DestroyImageView() {
			m_view.release();
		}

		void DestroySampler() {
			m_sampler.release();
		}

		void BlitTo(Image2D& dst, unsigned dst_mip, unsigned src_mip, vk::ImageLayout start_src_layout, vk::ImageLayout start_dst_layout,
			vk::ImageLayout final_src_layout, vk::ImageLayout final_dst_layout, vk::Filter filter, 
			std::optional<vk::Semaphore> wait_semaphore = std::nullopt, std::optional<vk::CommandBuffer> cmd_buf = std::nullopt);

		void CreateSampler();

		DescriptorGetInfo CreateDescriptorGetInfo(vk::ImageLayout layout, vk::DescriptorType type) const;

		void TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout, unsigned mip_level = 0, unsigned level_count = 1, vk::CommandBuffer buf = {});

		void TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout, 
			vk::AccessFlags src_access, vk::AccessFlags dst_access, vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage,  
			unsigned mip_level = 0, unsigned level_count = 1, vk::CommandBuffer buf = {});

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
		std::optional<SwapchainInvalidateEvent> m_swapchain_invalidate_event_data;
		EventListener m_frame_start_listener;
		EventListener m_swapchain_invalidate_listener;
	};


}