#include "EngineCore.h"
#include "Runtime/Function/Scene/SceneRuntime.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "Runtime/Function/Scene/GameProject.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Function/Input/Input.h"
#include "Runtime/Function/Audio/Audio.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"

#include "Runtime/Application/GameApp.h"

#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/ModelGpuBuilder.h"

using namespace AtomEngine;
namespace Engine
{
	AtomEngine::AtomEngine()
	{

	}

	AtomEngine::~AtomEngine()
	{
		if (mInitialized) Shutdown();
	}

	void AtomEngine::Run(GameApp& app, uint32_t maxFrames)
	{
		auto startupTime=std::chrono::steady_clock::now();
        gContext.Initialize();
        WriteLog(LogLevel::Info,"Startup","Platform initialization: "+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-startupTime).count())+" ms");
        { ScopedLogTimer timer("Startup","D3D12 initialization"); DX12Core::InitializeDx12(); }
		gContext.windowManager->onResize = [](uint32_t width, uint32_t height) { DX12Core::OnResize(width, height); };
		AssetManager::SetModelBuilder(BuildModelGpuResources);
		AssetManager::Initialize(L"");
		{ ScopedLogTimer timer("Startup","Renderer and shaders"); Renderer::Initialize(); }
		mInitialized = true;
		Log("Engine Start");
		auto window = gContext.windowManager;

		mRuntime=std::make_unique<SceneRuntime>();
        gContext.readScriptFields=[this](World &w,Entity e,size_t index){return mRuntime->ReadScriptFields(w,e,index);};
        { ScopedLogTimer timer("Startup","Application and scene"); app.Initialize(); }
        WriteLog(LogLevel::Info,"Startup","Ready in "+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-startupTime).count())+" ms");
        FlushLog();
		mLastTickTime = std::chrono::steady_clock::now();

		uint32_t frameCount = 0;
        bool firstFrame=true;
		while (!window->ShouldClose() && !app.Exit())
		{
			// Resize at a frame boundary, never re-enter rendering from WM_SIZE.
            window->ApplyPendingResize();
            const float deltaTime = CalculateDeltaTime();
			Tick(app,deltaTime);
            if(firstFrame) {
                firstFrame=false;
                WriteLog(LogLevel::Info,"Startup","First frame presented in "+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-startupTime).count())+" ms");
                FlushLog();
            }
			if (maxFrames && ++frameCount >= maxFrames) break;
		}

		DX12Core::gCommandManager.IdleGPU();
		mRuntime->Reset();
        gContext.readScriptFields={};
        mRuntime.reset();
        app.Shutdown();

		mIsQuit = true;
	}

	bool AtomEngine::Shutdown()
	{
		if (!mInitialized) return mIsQuit;
		mInitialized = false;
		DX12Core::gCommandManager.IdleGPU();
		Renderer::Shutdown();
		AssetManager::Shutdown();
		gContext.Shutdown();
		DX12Core::ShutdownDx12();
		gContext.windowManager->Shutdown();
		Log("[INFO]:Shutdown Engine\n");
		CloseLog();
		return mIsQuit;
	}

	void AtomEngine::LogicalTick(GameApp& app,float deltaTime)
	{
		gContext.input->Update();
        gContext.audio->Update();

		app.Update(deltaTime);
        if (app.IsPlaying() && !app.IsPaused() && ActiveProject.pendingScene && app.HasScene())
        {
            auto path = *ActiveProject.pendingScene; ActiveProject.pendingScene.reset();
            try
            {
                DX12Core::gCommandManager.IdleGPU();
                app.LoadProjectScene(ActiveProject.SceneRoot() / UTF8ToWString(path), [this] { mRuntime->Reset(); }); ActiveProject.currentScene = path;
                ActiveProject.lastLoadError.clear();
                WriteLog(LogLevel::Info, "Scene", "Loaded " + path);
            }
            catch (const std::exception& error) { ActiveProject.lastLoadError = error.what(); app.OnSceneLoadError(error.what()); WriteLog(LogLevel::Error, "Scene", error.what()); }
        }

		if(app.HasScene()){auto pointer=app.RuntimePointerPosition();mRuntime->Update(app.GetWorld(),app.RuntimeDeltaTime(deltaTime),app.IsPlaying(),app.AllowsRuntimeUiInput(),&pointer,app.IsPaused());app.OnRuntimeError(mRuntime->ScriptError());if(app.IsPlaying())ApplySceneCamera(app.GetWorld(),app.GetCamera());Renderer::Update(app.GetWorld(),app.IsPlaying() && !app.IsPaused()?app.RuntimeDeltaTime(deltaTime):0);}
        else mRuntime->Reset();
	}

	void AtomEngine::RenderTick(GameApp& app,float deltaTime)
	{

		app.Render();

		if (app.HasScene()) Renderer::Render(app.GetWorld(),app.GetCamera(), deltaTime);

		DX12Core::Present();
	}

	float AtomEngine::CalculateDeltaTime()
	{
		float deltaTime;
		{
			using namespace std::chrono;

			steady_clock::time_point tickTimePoint = steady_clock::now();
			duration<float> time_span = tickTimePoint - mLastTickTime;
			deltaTime = time_span.count();

			mLastTickTime = tickTimePoint;
		}
		return deltaTime;
	}
	void AtomEngine::CalculateFPS(float deltaTime)
	{
		mFrameCount++;

		if (mFrameCount == 1)
		{
			mAverageDuration = deltaTime;
		}
		else
		{
			mAverageDuration = mAverageDuration * (1.0f - kTargetFPS) + deltaTime * kTargetFPS;
		}

		mFps = 1.0f / mAverageDuration;
	}
	void AtomEngine::Tick(GameApp& app, float deltaTime)
	{
		app.BeginFrame();

		LogicalTick(app,deltaTime);
		CalculateFPS(deltaTime);



		RenderTick(app,deltaTime);
	}
}
