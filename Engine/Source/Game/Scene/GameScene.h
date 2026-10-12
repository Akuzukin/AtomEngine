#pragma once
#include "Runtime/Function/Scene/Scene.h"
#include "Runtime/Function/Camera/DebugCamera.h"
#include "Runtime/Function/Framework/ECS/ECSCommon.h"

#include "../fwd.h"

using namespace AtomEngine;

class ItemSystem;
class GoalSystem;
class ItemGoalEditor;

class GameScene : public Scene
{
public:
	GameScene(std::string_view name, AtomEngine::SceneManager& manager);
	bool Initialize() override;
	void Update(float deltaTime) override;
	void Render() override;
	void Shutdown() override;
	bool Exit() override;

private:
	Camera mGameCamera;
	// デバッグ用に主カメラと独立したビュー用カメラを追加
	Camera mDebugViewCamera;
	std::unique_ptr<DebugCamera> mDebugCamera;
	bool mUseDebugCamera = true;
	bool mIsModelRotating = true;
private:
	void DestroyGameObject();
	void ImGuiHandleObjects();

};