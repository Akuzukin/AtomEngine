#pragma once
#include "ImGuiCommon.h"
#include "EditorLayer.h"
#include "Runtime/Function/Camera/DebugCamera.h"
#include "Runtime/Application/GameApp.h"
namespace AtomEngine
{
	class EditorApp : public GameApp
	{
	public:
		void SelectObject(Entity e)
		{
			mEditor.Select(GetWorld(), e);
		}
		bool AllowsRuntimeUiInput() const override
		{
			return mEditor.ViewportHovered();
		}
		void OnRuntimeError(const std::string& error) override
		{
			mEditor.SetRuntimeError(error);
		}
		bool IsPlaying() const override
		{
			return mEditor.IsPlaying();
		}
		bool IsPaused() const override
		{
			return mEditor.IsPaused();
		}
		float RuntimeDeltaTime(float dt) const override
		{
			return mEditor.RuntimeDeltaTime(dt);
		}
		bool Exit() override
		{
			return mEditor.WantsExit();
		}
		void Initialize() override;
		void BeginFrame() override;
        void LoadProjectScene(const std::filesystem::path& path, const std::function<void()>& beforeCommit) override { GameApp::LoadProjectScene(path, beforeCommit); mEditor.RuntimeSceneLoaded(*mSceneManager.GetCurrentScene()); }
        void OnSceneLoadError(const std::string& error) override { mEditor.SetSceneLoadError(error); }
		void Update(float deltaTime) override;
		void Render() override;
		Vector2 RuntimePointerPosition() const override
		{
			return mEditor.RuntimePointerPosition();
		}
		void Shutdown() override;

	private:
		ImGuiCommon mImGui;
		std::unique_ptr<DebugCamera> mCameraController;
		std::weak_ptr<void> mCameraWorld;

	protected:
		EditorLayer mEditor;

	private:
		ColorBuffer mViewportTexture;
		DescriptorHandle mViewportDescriptor;
	};
} // namespace AtomEngine
