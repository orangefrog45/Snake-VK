#pragma once
#include "core/VkIncl.h"
#include "core/VkCommon.h"

namespace SNAKE {
	class Texture2D {
	public:
		void LoadFromFile(const std::string& filepath, vk::CommandPool pool);

		vk::ImageView GetImageView() {
			return *m_tex_view;
		}

		vk::Sampler GetSampler() {
			return *m_sampler;
		}

	private:
		void TransitionImageLayout(vk::Image& image, [[maybe_unused]] vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandPool pool);

		void CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, VmaAllocationCreateFlags flags=0);

		void CreateImageView();

		void CreateSampler();

		vk::UniqueSampler m_sampler;
		vk::Image m_tex_image;
		VmaAllocation m_allocation;
		vk::UniqueImageView m_tex_view;
	};
}