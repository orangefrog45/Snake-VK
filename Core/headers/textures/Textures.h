#pragma once
#include "core/VkIncl.h"

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
		void TransitionImageLayout(vk::UniqueImage& image, [[maybe_unused]] vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandPool pool);

		void CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
			vk::MemoryPropertyFlags properties, vk::UniqueImage& image, vk::UniqueDeviceMemory& memory);

		void CreateImageView();

		void CreateSampler();

		vk::UniqueSampler m_sampler;
		vk::UniqueImage m_tex_image;
		vk::UniqueDeviceMemory m_tex_mem;
		vk::UniqueImageView m_tex_view;
	};
}