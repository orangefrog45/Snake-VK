#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "resources/S_VkBuffer.h"
#include "resources/Images.h"
#include "rendering/VkRenderer.h"
#include "util/util.h"
#include "vk_mem_alloc.h"

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
	sampler_info.maxAnisotropy = VkContext::GetPhysicalDevice().properties.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
	sampler_info.unnormalizedCoordinates = false; // Coordinates are in range [0, 1], not [0, tex_pixel_extents]
	sampler_info.compareEnable = false;
	sampler_info.compareOp = vk::CompareOp::eAlways;
	sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler_info.mipLodBias = 0.f;
	sampler_info.minLod = 0.f;
	sampler_info.maxLod = (float)m_spec.mip_levels;

	auto [res, sampler] = VkContext::GetLogicalDevice().device->createSamplerUnique(sampler_info);
	SNK_CHECK_VK_RESULT(res);
	m_sampler = std::move(sampler);
}

void Image2D::BlitTo(Image2D& dst, unsigned dst_mip, unsigned src_mip, vk::ImageLayout start_src_layout, vk::ImageLayout start_dst_layout,
	vk::ImageLayout final_src_layout, vk::ImageLayout final_dst_layout, vk::Filter filter, std::optional<vk::Semaphore> wait_semaphore,
	std::optional<vk::CommandBuffer> cmd_buf) {
	vk::UniqueCommandBuffer temp;
	vk::CommandBuffer cmd;
	if (cmd_buf.has_value()) {
		cmd = cmd_buf.value();
	}
	else {
		temp = BeginSingleTimeCommands();
		cmd = *temp;
	}

	float src_div = glm::pow(2.f, (float)src_mip);
	float dst_div = glm::pow(2.f, (float)dst_mip);

	vk::ImageBlit blits;
	blits.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
	blits.srcOffsets[1] = vk::Offset3D{ glm::max((int32_t)((float)m_spec.size.x / src_div), 1), glm::max((int32_t)((float)m_spec.size.y / src_div), 1), 1 };
	blits.srcSubresource.aspectMask = m_spec.aspect_flags;
	blits.srcSubresource.mipLevel = src_mip;
	blits.srcSubresource.baseArrayLayer = 0;
	blits.srcSubresource.layerCount = 1;
	blits.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
	blits.dstOffsets[1] = vk::Offset3D{ glm::max((int32_t)((float)dst.m_spec.size.x / dst_div), 1), glm::max((int32_t)((float)dst.m_spec.size.y / dst_div), 1), 1 };
	blits.dstSubresource.aspectMask = dst.m_spec.aspect_flags;
	blits.dstSubresource.mipLevel = dst_mip;
	blits.dstSubresource.baseArrayLayer = 0;
	blits.dstSubresource.layerCount = 1;

	TransitionImageLayout(start_src_layout, vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eTransferRead,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, src_mip, 1, cmd);

	dst.TransitionImageLayout(start_dst_layout, vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, dst_mip, 1, cmd);

	cmd.blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, dst.m_image,
		vk::ImageLayout::eTransferDstOptimal, blits, filter);

	TransitionImageLayout(vk::ImageLayout::eTransferSrcOptimal, final_src_layout, vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eTransferRead,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, src_mip, 1, cmd);

	dst.TransitionImageLayout(vk::ImageLayout::eTransferDstOptimal, final_dst_layout, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite,
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, dst_mip, 1, cmd);

	if (cmd_buf.has_value())
		return;

	if (wait_semaphore.has_value())
		EndSingleTimeCommands(cmd, std::make_pair(wait_semaphore.value(), vk::PipelineStageFlagBits::eTransfer));
	else
		EndSingleTimeCommands(cmd);
}

void Image2D::CreateImage(VmaAllocationCreateFlags flags) {
	if (m_owns_image) {
		vk::ImageCreateInfo image_info{};
		image_info.imageType = vk::ImageType::e2D;
		image_info.extent.width = m_spec.size.x;
		image_info.extent.height = m_spec.size.y;
		image_info.extent.depth = 1;
		image_info.mipLevels = m_spec.mip_levels;
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

		SNK_CHECK_VK_RESULT(vmaCreateImage(VkContext::GetAllocator(), &im_info, &alloc_info, reinterpret_cast<VkImage*>(&m_image), &m_allocation, nullptr));
	}

	CreateImageView();
	CreateSampler();

	DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::CREATE);
}

void Image2D::GenerateMipmaps(vk::ImageLayout start_layout) {
	auto cmd = BeginSingleTimeCommands();
	for (uint32_t i = 1; i < m_spec.mip_levels; i++) {
		BlitTo(*this, i, i - 1, start_layout, start_layout, start_layout, start_layout, vk::Filter::eLinear, std::nullopt, *cmd);
	}
	EndSingleTimeCommands(*cmd);
}

void Image2D::DestroyImage() {
	if (m_image != VK_NULL_HANDLE && m_owns_image) {
		vmaDestroyImage(VkContext::GetAllocator(), m_image, m_allocation);
		m_image = VK_NULL_HANDLE;
	}

	if (m_sampler)
		m_sampler.reset();

	if (m_view)
		m_view.reset();

	DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::DELETE);
}

void Image2D::RefreshDescriptorGetInfo(DescriptorGetInfo& info) const {
	auto& image_info = std::get<vk::DescriptorImageInfo>(info.resource_info);
	image_info.imageView = *m_view;
	image_info.sampler = *m_sampler;
}

DescriptorGetInfo Image2D::CreateDescriptorGetInfo(vk::ImageLayout layout, vk::DescriptorType type) const {
	vk::DescriptorImageInfo image_info;
	image_info.setImageLayout(layout)
		.setImageView(GetImageView())
		.setSampler(GetSampler());

	DescriptorGetInfo ret;
	ret.resource_info = image_info;
	ret.get_info.type = type;

	return ret;
}

void Image2D::TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout,
	vk::AccessFlags src_access, vk::AccessFlags dst_access, vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage,
	unsigned mip_level, unsigned level_count, vk::CommandBuffer buf) {
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
	barrier.subresourceRange.baseMipLevel = mip_level;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.levelCount = level_count;
	barrier.subresourceRange.layerCount = 1;

	buf.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);

	if (temporary_buf)
		EndSingleTimeCommands(buf);
}

vk::DeviceMemory Image2D::GetMemory() {
	VmaAllocationInfo alloc_info;
	vmaGetAllocationInfo(VkContext::GetAllocator(), m_allocation, &alloc_info);
	return alloc_info.deviceMemory;
}

void Image2D::TransitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout, unsigned mip_level, unsigned level_count, vk::CommandBuffer buf) {
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

	TransitionImageLayout(old_layout, new_layout, src_access, dst_access, src_stage, dst_stage, mip_level, level_count, buf);
	
}

vk::UniqueImageView Image2D::CreateImageView(const Image2D& image, std::optional<vk::Format> fmt) {
	vk::ImageViewCreateInfo info{};
	info.image = image.m_image;
	info.viewType = vk::ImageViewType::e2D;
	info.format = fmt.has_value() ? fmt.value() : image.m_spec.format;
	info.subresourceRange.aspectMask = image.m_spec.aspect_flags;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.levelCount = image.m_spec.mip_levels;
	info.subresourceRange.layerCount = 1;

	auto [res, view] = VkContext::GetLogicalDevice().device->createImageViewUnique(info);
	SNK_CHECK_VK_RESULT(res);
	return std::move(view);
}


void Image2D::CreateImageView() {
	SNK_ASSERT(!m_view);
	m_view = CreateImageView(*this);
}

FullscreenImage2D::FullscreenImage2D(vk::ImageLayout initial_layout) : m_initial_layout(initial_layout) {
	m_swapchain_invalidate_listener.callback = [this](Event const* p_event) {
		auto* p_casted = dynamic_cast<SwapchainInvalidateEvent const*>(p_event);
		m_swapchain_invalidate_event_data = *p_casted;
	};

	m_frame_start_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
		if (!m_swapchain_invalidate_event_data.has_value() || !m_image)
			return;
		
		bool has_sampler = static_cast<bool>(m_sampler);
		bool has_view = static_cast<bool>(m_view);
		m_spec.size = m_swapchain_invalidate_event_data.value().new_swapchain_extents;

		DestroyImage();

		CreateImage();
		if (has_sampler) CreateSampler();
		if (has_view) CreateImageView();

		DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::CREATE);
		m_swapchain_invalidate_event_data.reset();

		TransitionImageLayout(vk::ImageLayout::eUndefined, m_initial_layout, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);
	};

	EventManagerG::RegisterListener<SwapchainInvalidateEvent>(m_swapchain_invalidate_listener);
	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}