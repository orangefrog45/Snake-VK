#pragma once
#include "core/DescriptorBuffer.h"
#include "core/Pipelines.h"
#include "core/VkCommands.h"
#include "resources/S_VkBuffer.h"
#include "System.h"

namespace SNAKE {
	enum class ParticleComputeShader {
		INIT,
		CELL_KEY_GENERATION,
		BITONIC_CELL_KEY_SORT,
		CELL_KEY_START_IDX_GENERATION,
	};

	struct PhysicsParamsUBO {
		float particle_restitution = 0.2f;
		float timestep = 1.f / 120.f;
		float friction_coefficient = 0.5;
		float repulsion_factor = 0.08f;
		float penetration_slop = 0.005f;
		float restitution_slop = 0.05f;
		float sleep_threshold = 0.05f;
	};

	class ParticleSystem : public System {
	public:
		void OnSystemAdd() override;

		void OnSystemRemove() override;

		void InitPipelines();

		void InitParticles();

		void InitializeSimulation();

		void UpdateParticles();

		void SetNumParticles(uint32_t num_particles);

		inline uint32_t GetNumParticles() const noexcept {
			return m_num_particles;
		}

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

		PhysicsParamsUBO parameters;

	private:
		void InitBuffers();

		uint32_t m_num_particles = 100'000;

		EventListener m_frame_start_listener;

		// Pipeline responsible for scene collisions with particles
		RtPipeline m_ptcl_rt_pipeline;

		std::unordered_map<ParticleComputeShader, ComputePipeline> m_compute_pipelines;

		std::shared_ptr<DescriptorSetSpec> m_ptcl_compute_descriptor_spec;
		std::shared_ptr<DescriptorSetSpec> mp_ptcl_rt_descriptor_spec;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_compute_descriptor_buffers;
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_rt_descriptor_buffers;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_buffers;
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_result_buffers;
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_cell_key_buffers;
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_cell_key_start_index_buffers;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_params_buffers;

		// The particle buffers containing last frames data relative to the current frame
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_ptcl_buffers_prev_frame;

		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
	};
}