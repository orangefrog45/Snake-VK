#include "pch/pch.h"
#include "assets/MeshProcessor.h"

using namespace SNAKE;

void MeshProcessor::Init() {
	m_compute_pipelines[MeshComputePipeline::TRIANGLE_AREA_IMAGE_WRITE].Init("");
	m_compute_pipelines[MeshComputePipeline::TRIANGLE_AREA_SUM].Init("");

	m_descriptor_buffer.SetDescriptorSpec(m_compute_pipelines[MeshComputePipeline::TRIANGLE_AREA_IMAGE_WRITE].pipeline_layout.GetDescriptorSetLayout(0));
	m_descriptor_buffer.CreateBuffer(MAX_FRAMES_IN_FLIGHT);
	//m_descriptor_buffer.LinkResource
}

//float MeshProcessor::CalculateMeshSurfaceArea(class MeshDataAsset* p_mesh) {

//}
