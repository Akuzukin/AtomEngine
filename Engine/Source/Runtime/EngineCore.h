#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
namespace AtomEngine { class SceneRuntime; }

class GameApp;

namespace Engine
{
	class AtomEngine
	{
	public:
		AtomEngine();
		virtual ~AtomEngine();

		void Run(GameApp& app, uint32_t maxFrames = 0);
		bool Shutdown();

		float FPS() const { return mFps; }

	protected:
		void LogicalTick(GameApp& app,float deltaTime);
		void RenderTick(GameApp& app,float deltaTime);
		void Tick(GameApp& app,float deltaTime);

	private:
		float CalculateDeltaTime();
		void CalculateFPS(float deltaTime);

	protected:
		std::unique_ptr<::AtomEngine::SceneRuntime> mRuntime;
		bool mIsQuit = false;
		bool mInitialized = false;
		std::chrono::steady_clock::time_point mLastTickTime{ std::chrono::steady_clock::now() };

		float mAverageDuration = 0.f;
		int   mFrameCount = 0;
		float mFps = 0;
		static constexpr float kTargetFPS = 1.f / 60.f;
	};
}
