#include "core/S_VkBuffer.h"
#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "images/Images.h"
#include "rendering/VkRenderer.h"
#include "util/util.h"

using namespace SNAKE;


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

void Image2D::BlitTo(Image2D& dst, vk::ImageLayout start_src_layout, vk::ImageLayout start_dst_layout,
	vk::ImageLayout final_src_layout, vk::ImageLayout final_dst_layout, std::optional<vk::Semaphore> wait_semaphore) {
	auto cmd = BeginSingleTimeCommands();
	vk::ImageBlit blits;
	auto offsets = util::array(vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ (int32_t)dst.GetSpec().size.x, (int32_t)dst.GetSpec().size.y, 1 });
	vk::ImageSubresourceLayers layers;
	layers.aspectMask = vk::ImageAspectFlagBits::eColor;
	layers.baseArrayLayer = 0;
	layers.layerCount = 1;
	layers.mipLevel = 0;

	blits.dstOffsets = offsets;
	blits.srcOffsets = offsets;
	blits.dstSubresource = layers;
	blits.srcSubresource = layers;

	TransitionImageLayout(start_src_layout, vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead,
		vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, *cmd);

	dst.TransitionImageLayout(start_dst_layout, vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, *cmd);

	cmd->blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, dst.m_image,
		vk::ImageLayout::eTransferDstOptimal, blits, vk::Filter::eNearest);

	TransitionImageLayout(vk::ImageLayout::eTransferSrcOptimal, final_src_layout, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eNone,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, *cmd);

	dst.TransitionImageLayout(vk::ImageLayout::eTransferDstOptimal, final_dst_layout, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput, *cmd);

	if (wait_semaphore.has_value())
		EndSingleTimeCommands(*cmd, std::make_pair(wait_semaphore.value(), vk::PipelineStageFlagBits::eTransfer));
	else
		EndSingleTimeCommands(*cmd);
}

void Image2D::CreateImage(VmaAllocationCreateFlags flags) {
	vk::ImageCreateInfo image_info{};
	image_info.imageType = vk::ImageType::e2D;
	image_info.extent.width = m_spec.size.x;
	image_info.extent.height = m_spec.size.y;
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
	if (m_image != VK_NULL_HANDLE)
		vmaDestroyImage(VulkanContext::GetAllocator(), m_image, m_allocation);

	if (m_sampler)
		m_sampler.release();

	if (m_view)
		m_view.release();
}

std::pair<vk::DescriptorGetInfoEXT, std::shared_ptr<vk::DescriptorImageInfo>> Image2D::CreateDescriptorGetInfo(vk::ImageLayout layout) const {
	auto image_info = std::make_shared<vk::DescriptorImageInfo>();
	image_info->setImageLayout(layout)
		.setImageView(GetImageView())
		.setSampler(GetSampler());

	vk::DescriptorGetInfoEXT image_descriptor_info{};
	image_descriptor_info.setType(vk::DescriptorType::eCombinedImageSampler)
		.setData(&*image_info);

	return std::make_pair(image_descriptor_info, image_info);
}

void Image2D::TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout,
	vk::AccessFlags src_access, vk::AccessFlags dst_access, vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage,
	vk::CommandBuffer buf) {
	bool temporary_buf = !buf;
	vk::UniqueCommandBuffer temp_handle;

	if (temporary_buf) {
		temp_handle = BeginSingleTimeCommands();
		buf = *temp_handle;
	}

	vk::ImageMemoryBarrier barrier{};
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored; // Not transfering queue family ownership so leave at ignore
	barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
	barrier.image = GetImage();
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.subresourceRange.aspectMask = m_spec.aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	buf.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);

	if (temporary_buf)
		EndSingleTimeCommands(buf);
}

void Image2D::TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::CommandBuffer buf) {
	vk::PipelineStageFlags src_stage = vk::PipelineStageFlagBits::eNone;
	vk::PipelineStageFlags dst_stage = vk::PipelineStageFlagBits::eNone;
	vk::AccessFlags src_access;
	vk::AccessFlags dst_access;

	if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
		// Don't have to wait on anything for this transition
		src_access = vk::AccessFlagBits::eNone;
		src_stage = vk::PipelineStageFlagBits::eTopOfPipe;

		dst_access = vk::AccessFlagBits::eTransferWrite;
		dst_stage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		// Wait for transfer to occur before transition
		src_access = vk::AccessFlagBits::eTransferWrite;
		src_stage = vk::PipelineStageFlagBits::eTransfer;

		dst_access = vk::AccessFlagBits::eShaderRead;
		dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if ((new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal || new_layout == vk::ImageLayout::eDepthAttachmentOptimal)) {
		src_access = vk::AccessFlagBits::eNone;
		src_stage = vk::PipelineStageFlagBits::eTopOfPipe;

		dst_access = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		dst_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
	}
	else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eColorAttachmentOptimal) {
		src_access = vk::AccessFlagBits::eNone;
		src_stage = vk::PipelineStageFlagBits::eTopOfPipe;

		dst_access = vk::AccessFlagBits::eColorAttachmentWrite;
		dst_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	}
	else if (old_layout == vk::ImageLayout::eColorAttachmentOptimal && new_layout == vk::ImageLayout::ePresentSrcKHR) {
		src_access = vk::AccessFlagBits::eColorAttachmentWrite;
		src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		dst_access = vk::AccessFlagBits::eNone;
		dst_stage = vk::PipelineStageFlagBits::eBottomOfPipe;
	}
	else {
		SNK_BREAK("Unsupported layout transition");
	}

	TransitionImageLayout(old_layout, new_layout, src_access, dst_access, src_stage, dst_stage, buf);
	
}

vk::UniqueImageView Image2D::CreateImageView(const Image2D& image, std::optional<vk::Format> fmt) {
	vk::ImageViewCreateInfo info{};
	info.image = image.m_image;
	info.viewType = vk::ImageViewType::e2D;
	info.format = fmt.has_value() ? fmt.value() : image.m_spec.format;
	info.subresourceRange.aspectMask = image.m_spec.aspect_flags;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.layerCount = 1;

	auto [res, view] = VulkanContext::GetLogicalDevice().device->createImageViewUnique(info);
	SNK_CHECK_VK_RESULT(res);
	return std::move(view);
}


void Image2D::CreateImageView() {
	m_view = CreateImageView(*this);
}

FullscreenImage2D::FullscreenImage2D() {
	m_swapchain_invalidate_listener.callback = [this](Event const* p_event) {
		auto* p_casted = dynamic_cast<SwapchainInvalidateEvent const*>(p_event);
		bool has_sampler = static_cast<bool>(m_sampler);
		bool has_view = static_cast<bool>(m_view);
		m_spec.size = p_casted->new_swapchain_extents;

		DestroyImage();

		CreateImage();
		if (has_sampler) CreateSampler();
		if (has_view) CreateImageView();
	};

	EventManagerG::RegisterListener<SwapchainInvalidateEvent>(m_swapchain_invalidate_listener);
}