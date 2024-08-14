#include "EditorLayer.h"
#include "components/Components.h"
#include "rendering/VkRenderer.h"
#include "imgui.h"


using namespace SNAKE;

void EditorLayer::OnInit() {
	renderer.Init(*p_window);

	scene.AddDefaultSystems();
	scene.CreateEntity()->AddComponent<MeshComponent>();

	p_cam_ent = scene.CreateEntity();
	p_cam_ent->AddComponent<CameraComponent>()->MakeActive();

}

void EditorLayer::OnUpdate() {
	glm::vec3 move{ 0,0,0 };
	auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();

	if (p_window->input.IsMouseDown(1)) {
		auto delta = p_window->input.GetMouseDelta();
		glm::vec3 forward_rotate_y = glm::angleAxis(-0.01f * delta.x, glm::vec3{ 0, 1, 0 }) * p_transform->forward;
		glm::vec3 forward_rotate_x = glm::angleAxis(-0.01f * delta.y, p_transform->right) * forward_rotate_y;
		p_transform->LookAt(p_transform->GetPosition() + forward_rotate_x);
	}

	if (p_window->input.IsKeyDown('w'))
		move += p_transform->forward;

	if (p_window->input.IsKeyDown('s'))
		move -= p_transform->forward;

	if (p_window->input.IsKeyDown('d'))
		move += p_transform->right;

	if (p_window->input.IsKeyDown('a'))
		move -= p_transform->right;

	p_transform->SetPosition(p_transform->GetPosition() + move * 0.1f);
}

void EditorLayer::OnRender() {
	uint32_t image_index;
	vk::Semaphore image_avail_semaphore = VkRenderer::AcquireNextSwapchainImage(*p_window, image_index);
	renderer.RenderScene(&scene, *p_window->GetVkContext().swapchain_images[image_index], image_avail_semaphore);
}

void EditorLayer::OnImGuiRender() {
	ImGui::ShowDemoWindow();
}

void EditorLayer::OnShutdown() {}