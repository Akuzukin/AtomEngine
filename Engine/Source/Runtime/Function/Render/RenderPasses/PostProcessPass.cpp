#include "PostProcessPass.h"
#include "Runtime/Function/Render/SelectionRenderer.h"
#include "Runtime/Function/Render/DebugDraw.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Function/Render/Particle/ParticleSystem.h"
#include "Runtime/Function/Render/PostEffect/MotionBlur.h"

#include "imgui.h"
#include "Runtime/Function/Global/GlobalContext.h"

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

	void PostProcessPass::Shutdown()
	{
		ParticleSystem::Shutdown();
		if (mPostEffect) mPostEffect->Shutdown();
		mPostEffect.reset();
		mGridRenderer.reset();
	}

	void PostProcessPass::Update(GraphicsContext& Context, float deltaTime)
	{
		ParticleSystem::Update(Context.GetComputeContext(), deltaTime);
#ifndef RELEASE

		if(ImGui::GetCurrentContext() && gContext.showRenderDiagnostics){
        ImGui::Begin("Motion Blur");
		ImGui::Checkbox("Enable", &mIsMotionBlurEnabled);
		MotionBlur::SedtEnableMotionBlur(mIsMotionBlurEnabled);
		ImGui::End();
        }
#endif // !RELEASE

	}

	void PostProcessPass::Render(GraphicsContext& gfxContext)
	{

		MotionBlur::GenerateCameraVelocityBuffer(gfxContext, *mCamera, true);


		ParticleSystem::Render(gfxContext, *mCamera, *mRTV, *mDSV);

		MotionBlur::RenderCameraBlur(gfxContext, *mCamera);
		MotionBlur::RenderObjectBlur(gfxContext, gVelocityBuffer);

		if (gContext.showGrid) mGridRenderer->Render(gfxContext, mCamera, *mRTV, *mDSV, mViewport, mScissor);
		if (gContext.showColliders) SelectionRenderer::DrawColliders(*mWorld,*mCamera);
        gfxContext.SetRenderTarget(gSceneColorBuffer.GetRTV(),gSceneDepthBuffer.GetDSV_DepthReadOnly());
        gfxContext.TransitionResource(gSceneDepthBuffer,D3D12_RESOURCE_STATE_DEPTH_READ,true);
        DebugDraw::Render(gfxContext);
        SelectionRenderer::Render(gfxContext,*mWorld,*mCamera);
		mPostEffect->Render(gfxContext.GetComputeContext());
	}
}

