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
std::vector<GraphUse> PostProcessPass::GraphResources() const
{
    return {{&gSceneColorBuffer, GraphAccess::ReadWrite, D3D12_RESOURCE_STATE_RENDER_TARGET},
            {&gSceneDepthBuffer, GraphAccess::Read, D3D12_RESOURCE_STATE_DEPTH_READ},
            {&gVelocityBuffer, GraphAccess::Write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS},
            {&gSelectionMask, GraphAccess::Write, D3D12_RESOURCE_STATE_RENDER_TARGET},
            {&gViewExposure, GraphAccess::ReadWrite, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE},
            {&gHistogram, GraphAccess::Write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS},
            {&gPostEffectsBuffer, GraphAccess::Write, D3D12_RESOURCE_STATE_UNORDERED_ACCESS}};
}
void PostProcessPass::Initialize()
{

    mPostEffect = std::make_unique<PostEffect>();
    mPostEffect->Initialize();

#ifndef ATOM_GAME_RUNTIME
    mGridRenderer = std::make_unique<GridRenderer>();
    mGridRenderer->Initialize();
#endif
}

void PostProcessPass::Shutdown()
{
    if (mPostEffect)
        mPostEffect->Shutdown();
    mPostEffect.reset();
    mGridRenderer.reset();
}

void PostProcessPass::Update(GraphicsContext &Context, float deltaTime)
{
    // Particle simulation advances once in Renderer::Update, independent of view count.
#if !defined(RELEASE) && !defined(ATOM_GAME_RUNTIME)

    if (ImGui::GetCurrentContext() && gContext.showRenderDiagnostics)
    {
        ImGui::Begin("Motion Blur");
        ImGui::Checkbox("Enable", &mIsMotionBlurEnabled);
        MotionBlur::SedtEnableMotionBlur(mIsMotionBlurEnabled);
        ImGui::End();
    }
#endif // !RELEASE
}

void PostProcessPass::Render(GraphicsContext &gfxContext)
{

    MotionBlur::SedtEnableMotionBlur(mIsMotionBlurEnabled);
    MotionBlur::GenerateCameraVelocityBuffer(gfxContext, GetViewBuffers().reprojection, mCamera->GetNearClip(),
                                             mCamera->GetFarClip(), true);

    ParticleSystem::Render(gfxContext, *mCamera, *mRTV, *mDSV);

    MotionBlur::RenderCameraBlur(gfxContext, GetViewBuffers().reprojection, mCamera->GetNearClip(),
                                 mCamera->GetFarClip());
    MotionBlur::RenderObjectBlur(gfxContext, gVelocityBuffer);

#ifndef ATOM_GAME_RUNTIME
    if (gContext.showGrid)
        mGridRenderer->Render(gfxContext, mCamera, *mRTV, *mDSV, mViewport, mScissor);
    if (gContext.showColliders)
        SelectionRenderer::DrawColliders(*mWorld, *mCamera);
#endif
    gfxContext.SetRenderTarget(gSceneColorBuffer.GetRTV(), gSceneDepthBuffer.GetDSV_DepthReadOnly());
    gfxContext.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ, true);
    DebugDraw::Render(gfxContext);
#ifndef ATOM_GAME_RUNTIME
    SelectionRenderer::Render(gfxContext, *mWorld, *mCamera);
#endif
    mPostEffect->SetExposure(mWorld->settings.exposure, mWorld->settings.autoExposure);
    mPostEffect->Render(gfxContext.GetComputeContext());
}
} // namespace AtomEngine
