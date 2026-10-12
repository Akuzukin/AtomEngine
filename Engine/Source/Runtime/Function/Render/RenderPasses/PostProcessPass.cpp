#include "PostProcessPass.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Function/Render/Particle/ParticleSystem.h"
#include "Runtime/Function/Render/PostEffect/TemporalAA.h"
#include "Runtime/Function/Render/PostEffect/MotionBlur.h"

#include "imgui.h"

namespace AtomEngine
{
	void PostProcessPass::Initialize()
	{
		ParticleSystem::Initialize();

		mPostEffect = std::make_unique<PostEffect>();
		mPostEffect->Initialize();

		mGridRenderer = std::make_unique<GridRenderer>();
		mGridRenderer->Initialize();
	}

	void PostProcessPass::Update(GraphicsContext& Context, float deltaTime)
	{
		ParticleSystem::Update(Context.GetComputeContext(), deltaTime);
#ifndef RELEASE

		TemporalAA::ImGuiSettingsWindow();
		ImGui::Begin("Motion Blur");
		ImGui::Checkbox("Enable", &mIsMotionBlurEnabled);
		MotionBlur::SedtEnableMotionBlur(mIsMotionBlurEnabled);
		ImGui::End();
#endif // !RELEASE

	}

	void PostProcessPass::Render(GraphicsContext& gfxContext)
	{

		MotionBlur::GenerateCameraVelocityBuffer(gfxContext, *mCamera, true);

		TemporalAA::ResolveImage(gfxContext);

		ParticleSystem::Render(gfxContext, *mCamera, *mRTV, *mDSV);

		MotionBlur::RenderCameraBlur(gfxContext, *mCamera);
		MotionBlur::RenderObjectBlur(gfxContext, gVelocityBuffer);

#ifdef _DEBUG
		//mGridRenderer->Render(gfxContext, mCamera, *mRTV, *mDSV, mViewport, mScissor);
#endif
		mPostEffect->Render(gfxContext.GetComputeContext());
	}
}

