#pragma once
#include "core/DescriptorBuffer.h"
#include "core/Pipelines.h"
#include "core/VkCommands.h"
#include "resources/S_VkBuffer.h"
#include "System.h"

namespace SNAKE {
	enum class ParticleComputeShader {
		INIT,
		UPDATE,
	};

	class ParticleSystem : public System {
	public:
		void OnSystemAdd() override;

		void OnSystemRemove() override;

		void InitPipelines();

		void InitParticles();

		void UpdateParticles();

		const S_VkBuffer& GetParticleBuffer(FrameInFlightIndex idx) {
			return m_ptcl_buffers[idx];
		}

		const S_VkBuffer& GetPrevFrameParticleBuffer(FrameInFlightIndex idx) {
			return m_ptcl_buffers_prev_frame[idx];
		}

		uint32_t GetParticleCount() const {
			return m_num_particles;
		}

		// If particles are currently updating each frame
		bool active = false;

	private:
		uint32_t m_num_particles = 10'000'000;

		EventListener m_frame_start_listener;

		// Pipeline responsible for scene collisions with particles
		RtPipeline m_ptcl_rt_pipeline;

		std::unordered_map<ParticleComputeShader, ComputePipeline> m_compute_pipelines;

		std::weak_ptr<const DescriptorSetSpec> m_ptcl_compute_descriptor_spec;

		std::shared_ptr<DescriptorSetSpec> mp_ptcl_rt_descriptor_spec;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_compute_descriptor_buffers;
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_rt_descriptor_buffers;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_buffers;
		// The particle buffers containing last frames data relative to the current frame
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_buffers_prev_frame;

		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
	};
}