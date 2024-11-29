#pragma once
#include "core/Pipelines.h"

namespace SNAKE {
	class MeshProcessor {
	public:
		void Init();

		float CalculateMeshSurfaceArea(class MeshDataAsset* p_mesh);
	private:
		enum MeshComputePipeline {
			TRIANGLE_AREA_IMAGE_WRITE,
			TRIANGLE_AREA_SUM
		};

		std::unordered_map<MeshComputePipeline, ComputePipeline> m_compute_pipelines;
		DescriptorBuffer m_descriptor_buffer;
	};
}