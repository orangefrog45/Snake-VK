#pragma once
#include "events/EventManager.h"
#include "events/EventsCommon.h"
#include "core/VkIncl.h"

namespace SNAKE {
	struct DescriptorGetInfo {
		void Bind() {
			switch (get_info.type) {
			case vk::DescriptorType::eCombinedImageSampler:
				get_info.data.pCombinedImageSampler = &std::get<vk::DescriptorImageInfo>(resource_info);
				break;
			case vk::DescriptorType::eStorageBuffer:
				get_info.data.pStorageBuffer = &std::get<vk::DescriptorAddressInfoEXT>(resource_info);
				break;
			case vk::DescriptorType::eUniformBuffer:
				get_info.data.pUniformBuffer = &std::get<vk::DescriptorAddressInfoEXT>(resource_info);
				break;
			case vk::DescriptorType::eAccelerationStructureKHR:
				get_info.data.accelerationStructure = std::get<vk::DeviceAddress>(resource_info);
				break;
			case vk::DescriptorType::eStorageImage:
				get_info.data.pStorageImage = &std::get<vk::DescriptorImageInfo>(resource_info);
				break;
			default:
				SNK_BREAK("DescriptorGetInfo::Bind failed, unsupported descriptor type");
			}
		}

		vk::DescriptorGetInfoEXT get_info;
		std::variant<vk::DescriptorImageInfo, vk::DescriptorAddressInfoEXT, vk::DeviceAddress> resource_info;
	};

	struct S_VkResourceEvent : public Event {
		enum class ResourceEventType {
			DELETE,
			UPDATE,
			CREATE
		} event_type;

		S_VkResourceEvent(struct S_VkResource* _resource, ResourceEventType type) : event_type(type), p_resource(_resource) {}

		S_VkResource* p_resource = nullptr;
	};

	struct S_VkResource {
		virtual ~S_VkResource() = default;

		// Modifies an existing DescriptorGetInfo struct to update things like address locations for descriptor resources
		virtual void RefreshDescriptorGetInfo(DescriptorGetInfo& info) const = 0;
	protected:

		void DispatchResourceEvent(S_VkResourceEvent::ResourceEventType type) {
			EventManagerG::DispatchEvent(S_VkResourceEvent{ this, type });
		}
	};
}