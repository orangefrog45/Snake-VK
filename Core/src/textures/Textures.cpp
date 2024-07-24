#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "textures/Textures.h"
#include "util/Logger.h"
#include "util/util.h"

#include "stb_image.h"


using namespace SNAKE;

void Texture2D::LoadFromFile(const std::string& filepath, vk::CommandPool pool) {
	int width, height, channels;

	// Force 4 channels as most GPUs only support these as samplers
	stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
	vk::DeviceSize image_size = width * height * 4;

	SNK_ASSERT(pixels, "Image file loaded");

	S_VkBuffer staging_buffer{};
	staging_buffer.CreateBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

	void* p_data = nullptr;
	vk::DeviceMemory staging_mem = staging_buffer.alloc_info.deviceMemory;

	SNK_CHECK_VK_RESULT(
		VulkanContext::GetLogicalDevice().device->mapMemory(staging_mem, 0, image_size, {}, &p_data)
	);
	memcpy(p_data, pixels, (size_t)image_size);
	VulkanContext::GetLogicalDevice().device->unmapMemory(staging_mem);

	stbi_image_free(pixels);

	vk::Format fmt = vk::Format::eR8G8B8A8Srgb;

	CreateImage(width, height, fmt, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

	TransitionImageLayout(m_tex_image, fmt, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, pool);
	CopyBufferToImage(staging_buffer.buffer, *m_tex_image, width, height, pool);
	TransitionImageLayout(m_tex_image, fmt, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, pool);

	CreateImageView();
	CreateSampler();
}

void Texture2D::CreateSampler() {
	vk::SamplerCreateInfo sampler_info{};
	sampler_info.magFilter = vk::Filter::eLinear;
	sampler_info.minFilter = vk::Filter::eLinear;
	sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
	sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
	sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
	sampler_info.anisotropyEnable = true;
	sampler_info.maxAnisotropy = VulkanContext::GetPhysicalDevice().properties.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
	sampler_info.unnormalizedCoordinates = false; // Coordinates are in range [0, 1], not [0, tex_pixel_extents]
	sampler_info.compareEnable = false;
	sampler_info.compareOp = vk::CompareOp::eAlways;
	sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler_info.mipLodBias = 0.f;
	sampler_info.minLod = 0.f;
	sampler_info.maxLod = 0.f;

	auto [res, sampler] = VulkanContext::GetLogicalDevice().device->createSamplerUnique(sampler_info);
	SNK_CHECK_VK_RESULT(res);
	m_sampler = std::move(sampler);
}

void Texture2D::CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, VmaAllocationCreateFlags flags) {
	vk::ImageCreateInfo image_info{};
	image_info.imageType = vk::ImageType::e2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.usage = usage;
	image_info.sharingMode = vk::SharingMode::eExclusive; // Image only used by one queue family
	image_info.samples = vk::SampleCountFlagBits::e1; // Multisampling config, ignore

	auto im_info = static_cast<VkImageCreateInfo>(image_info);

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
	alloc_info.flags = flags;

	SNK_CHECK_VK_RESULT(vmaCreateImage(VulkanContext::GetAllocator(), &im_info, &alloc_info, reinterpret_cast<VkImage*>(&*m_tex_image), &m_allocation, nullptr));
}


void Texture2D::TransitionImageLayout(vk::UniqueImage& image, [[maybe_unused]] vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandPool pool) {
	auto cmd_buf = BeginSingleTimeCommands(pool);

	vk::PipelineStageFlags src_stage = vk::PipelineStageFlagBits::eNone;
	vk::PipelineStageFlags dst_stage = vk::PipelineStageFlagBits::eNone;

	vk::ImageMemoryBarrier barrier{};

	if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
		// Don't have to wait on anything for this transition
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		src_stage = vk::PipelineStageFlagBits::eTopOfPipe;

		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		dst_stage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		// Wait for transfer to occur before transition
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		src_stage = vk::PipelineStageFlagBits::eTransfer;

		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else {
		SNK_BREAK("Unsupported layout transition");
	}

	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored; // Not transfering queue family ownership so leave at ignore
	barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
	barrier.image = *image;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	cmd_buf->pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);

	EndSingleTimeCommands(*cmd_buf);
}

void Texture2D::CreateImageView() {
	vk::ImageViewCreateInfo info{};
	info.image = *m_tex_image;
	info.viewType = vk::ImageViewType::e2D;
	info.format = vk::Format::eR8G8B8A8Srgb;
	info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.layerCount = 1;

	auto [res, view] = VulkanContext::GetLogicalDevice().device->createImageViewUnique(info);
	SNK_CHECK_VK_RESULT(res);
	m_tex_view = std::move(view);
}