#include "EditorApp.h"

namespace AtomEngine
{
	class EditorScene final : public Scene
	{
	public:
		explicit EditorScene(SceneManager& manager) : Scene("Editor", manager)
		{
			mCamera = &mEditorCamera;
			mEditorCamera.SetLookAt(Vector3(0.0f, 2.0f, -5.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
			mEditorCamera.Update();
		}
		void Update(float) override { mEditorCamera.Update(); }
	private:
		Camera mEditorCamera;
	};

	void EditorApp::Initialize()
	{
		mSceneManager.PushScene(std::make_unique<EditorScene>(mSceneManager));
	}

	void EditorApp::Update(float deltaTime)
	{
		mSceneManager.Update(deltaTime);
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
		mSceneManager.Shutdown();
	}
}
