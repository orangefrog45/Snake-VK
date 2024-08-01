#pragma once
#include "core/VkIncl.h"
#include "core/VkCommon.h"

namespace SNAKE {
	class Image2D {
	public:
		Image2D() = default;

		Image2D(const Image2D& other) = delete;

		Image2D(Image2D&& other) = delete;

		Image2D& operator=(const Image2D& other) = delete;

		~Image2D() {
			DestroyImage();
		}

		void DestroyImage();

		void LoadFromFile(const std::string& filepath, vk::CommandPool pool);

		void CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, VmaAllocationCreateFlags flags=0);

		void CreateImageView(vk::ImageAspectFlags aspect_flags, vk::Format format);

		void CreateSampler();

		static void TransitionImageLayout(const vk::Image& image, vk::ImageAspectFlags aspect_flags, [[maybe_unused]] vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandPool pool, vk::CommandBuffer buf = {});
		
		vk::ImageView GetImageView() {
			return *m_view;
		}

		vk::Sampler GetSampler() {
			return *m_sampler;
		}

		vk::Image GetImage() {
			return m_image;
		}

	private:
		vk::UniqueSampler m_sampler;
		vk::Image m_image;
		vk::UniqueImageView m_view;
		VmaAllocation m_allocation;
	};
}