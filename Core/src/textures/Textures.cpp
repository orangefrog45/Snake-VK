#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "textures/Textures.h"
#include "core/S_VkBuffer.h"
#include "util/Logger.h"
#include "util/util.h"

#include "stb_image.h"


using namespace SNAKE;

void Image2D::LoadFromFile(const std::string& filepath, vk::CommandPool pool) {
	int width, height, channels;
	// Force 4 channels as most GPUs only support these as samplers
	stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
	vk::DeviceSize image_size = width * height * 4;
	SNK_ASSERT(pixels, "Image file loaded");

	S_VkBuffer staging_buffer{};
	staging_buffer.CreateBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	void* p_data = staging_buffer.Map();
	memcpy(p_data, pixels, (size_t)image_size);
	staging_buffer.Unmap();

	stbi_image_free(pixels);

	m_spec.format = vk::Format::eR8G8B8A8Srgb;
	m_spec.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
	m_spec.width = width;
	m_spec.height = height;
	m_spec.tiling = vk::ImageTiling::eOptimal;

	CreateImage();

	TransitionImageLayout(m_image, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, pool);
	CopyBufferToImage(staging_buffer.buffer, m_image, width, height, pool);
	TransitionImageLayout(m_image, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, pool);

	CreateImageView(vk::ImageAspectFlagBits::eColor);
	CreateSampler();
}

Image2D::Image2D(const Image2DSpec& spec) {
	SetSpec(spec);
}

void Image2D::CreateSampler() {
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

void Image2D::CreateImage(VmaAllocationCreateFlags flags) {
	vk::ImageCreateInfo image_info{};
	image_info.imageType = vk::ImageType::e2D;
	image_info.extent.width = m_spec.width;
	image_info.extent.height = m_spec.height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = m_spec.format;
	image_info.tiling = m_spec.tiling;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.usage = m_spec.usage;
	image_info.sharingMode = vk::SharingMode::eExclusive; // Image only used by one queue family
	image_info.samples = vk::SampleCountFlagBits::e1; // Multisampling config, ignore

	auto im_info = static_cast<VkImageCreateInfo>(image_info);

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
	alloc_info.flags = flags;

	SNK_CHECK_VK_RESULT(vmaCreateImage(VulkanContext::GetAllocator(), &im_info, &alloc_info, reinterpret_cast<VkImage*>(&m_image), &m_allocation, nullptr));
}

void Image2D::DestroyImage() {
	if (m_image)
		vmaDestroyImage(VulkanContext::GetAllocator(), m_image, m_allocation);

	if (m_sampler)
		m_sampler.release();

	if (m_view)
		m_view.release();
}


void Image2D::TransitionImageLayout(const vk::Image& image, vk::ImageAspectFlags aspect_flags, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandPool pool, vk::CommandBuffer buf) {
	bool temporary_buf = !buf;
	vk::UniqueCommandBuffer temp_handle;

	if (temporary_buf) {
		temp_handle = std::move(BeginSingleTimeCommands(pool));
		buf = *temp_handle;
	}

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
	else if (old_layout == vk::ImageLayout::eUndefined && (new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal || new_layout == vk::ImageLayout::eDepthAttachmentOptimal)) {
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		src_stage = vk::PipelineStageFlagBits::eTopOfPipe;

		barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		dst_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
	}
	else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eColorAttachmentOptimal) {
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		src_stage = vk::PipelineStageFlagBits::eTopOfPipe;

		barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		dst_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	}
	else if (old_layout == vk::ImageLayout::eColorAttachmentOptimal && new_layout == vk::ImageLayout::ePresentSrcKHR) {
		barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		barrier.dstAccessMask = vk::AccessFlagBits::eNone;
		dst_stage = vk::PipelineStageFlagBits::eBottomOfPipe;
	}
	else {
		SNK_BREAK("Unsupported layout transition");
	}

	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored; // Not transfering queue family ownership so leave at ignore
	barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	buf.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);

	if (temporary_buf)
		EndSingleTimeCommands(buf);
}

void Image2D::CreateImageView(vk::ImageAspectFlags aspect_flags) {
	vk::ImageViewCreateInfo info{};
	info.image = m_image;
	info.viewType = vk::ImageViewType::e2D;
	info.format = m_spec.format;
	info.subresourceRange.aspectMask = aspect_flags;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.layerCount = 1;

	auto [res, view] = VulkanContext::GetLogicalDevice().device->createImageViewUnique(info);
	SNK_CHECK_VK_RESULT(res);
	m_view = std::move(view);
}