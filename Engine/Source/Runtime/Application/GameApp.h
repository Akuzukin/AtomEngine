#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Input/Input.h"

#include "Runtime/Platform/DirectX12/D3dUtility/d3dInclude.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include "Runtime/Platform/DirectX12/Pipeline/RootSignature.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Function/Scene/SceneManager.h"

using namespace AtomEngine;

class GameApp
{
public:
	virtual ~GameApp() = default;

	virtual void Initialize() = 0;
	virtual void BeginFrame() {}
    virtual void LoadProjectScene(const std::filesystem::path& path, const std::function<void()>& beforeCommit) { mSceneManager.GetCurrentScene()->Load(path, beforeCommit); }
    virtual void OnSceneLoadError(const std::string& error) { OnRuntimeError(error); }
    virtual bool AllowsRuntimeUiInput()const{return true;}
    virtual Vector2 RuntimePointerPosition() const { auto& m=Input::GetInstance()->GetMouseState(); return Vector2(float(m.positionX),float(m.positionY)); }
    virtual void OnRuntimeError(const std::string&) {}
    virtual bool IsPlaying() const { return true; }
    virtual bool IsPaused() const {return false;}
    virtual float RuntimeDeltaTime(float dt) const {return dt;}
	bool HasScene() const { return mSceneManager.GetCurrentScene() != nullptr; }
	virtual void Update(float deltaTime) = 0;
	virtual void Render() = 0;
	virtual void Shutdown() = 0;
	virtual bool Exit();
	World& GetWorld()
	{
		return mSceneManager.GetCurrentScene()->GetWorld();
	}
    Camera& GetCamera()
	{
        return mSceneManager.GetCurrentScene()->GetCamera();
	}
protected:
	SceneManager mSceneManager;
};
