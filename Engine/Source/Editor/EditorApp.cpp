#include "EditorApp.h"

namespace AtomEngine
{
	bool EditorApp::Initialize()
	{
		return true;
	}

	void EditorApp::Update(float deltaTime)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGui::Begin("DockSpace Demo", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar);

		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		ImGui::End();
		ImGui::PopStyleVar(2);
	}
	void EditorApp::Render()
	{

	}
	void EditorApp::Shutdown()
	{

	}
}