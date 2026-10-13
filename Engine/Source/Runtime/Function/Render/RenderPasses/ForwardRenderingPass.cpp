#include "ForwardRenderingPass.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"
#include "Runtime/Function/Light/LightManager.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Function/Render/PostEffect/SSAO.h"

#include "imgui.h"
#include "Runtime/Function/Global/GlobalContext.h"

namespace AtomEngine
{
ForwardRenderingPass::ForwardRenderingPass()
{
    mSkybox.Initialize();
    mSkybox.SetEnvironmentMap(L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KEnvHDR.dds");
    mSkybox.SetBRDF_LUT(L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KBrdf.dds");
    mSkybox.SetIBLTextures(L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KDiffuseHDR.dds",
                           L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KSpecularHDR.dds");
    mShadowSrvGpuHandle = Renderer::GetTextureHeap().Alloc();
    DX12Core::gDevice->CopyDescriptorsSimple(1, mShadowSrvGpuHandle, gShadowBuffer.GetSRV(),
                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ForwardRenderingPass::~ForwardRenderingPass()
{
    mSkybox.Shutdown();
    Renderer::GetTextureHeap().Free(mShadowSrvGpuHandle);
}

void ForwardRenderingPass::Render(GraphicsContext &gfxContext)
{
    ASSERT(mWorld != nullptr);

    const D3D12_VIEWPORT &viewport = mViewport;
    const D3D12_RECT &scissor = mScissor;
    auto lights = mWorld->View<DirectionalLightComponent, TransformComponent>();
    Entity sun = entt::null;
    for (auto e : lights)
        if (sun == entt::null || mWorld->GetComponent<PersistentIdComponent>(e).value <
                                     mWorld->GetComponent<PersistentIdComponent>(sun).value)
            sun = e;
    if (sun != entt::null)
    {
        auto &light = mWorld->GetComponent<DirectionalLightComponent>(sun);
        mSunColor = light.color;
        mSunLightIntensity = light.enabled ? light.intensity : 0;
        mSunDirection = mWorld->GetComponent<TransformComponent>(sun).GetMatrix().GetRotation() * Vector3::FORWARD;
        mShadowsEnabled = light.enabled && light.shadows;
        mAutoFitShadow = light.autoFitShadow;
    }
    else
    {
        mSunLightIntensity = 0;
        mShadowsEnabled = false;
    }
#ifndef RELEASE
    if (ImGui::GetCurrentContext() && gContext.showRenderDiagnostics)
    {
        ImGui::Begin("Forward Rendering");
        ImGui::TextUnformatted("Configure Sun in the Hierarchy / Inspector");
        ImGui::DragFloat("IBLFactor", &mIBLFactor, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("IBLBias", &mIBLBias, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat3("SunCenter", mShadowCenter.ptr(), 0.1f);
        ImGui::DragFloat3("ShadowDim", mShadowDim.ptr(), 0.1f, 0.1f);
        ImGui::Text("Shadows: %s | Auto fit: %s", mShadowsEnabled ? "On" : "Off", mAutoFitShadow ? "On" : "Off");
        ImGui::Checkbox("EnableIBL", &mEnableIBL);
        ImGui::End();

        ImGui::Begin("Shadow Srv");
        ImGui::Image((ImTextureID)mShadowSrvGpuHandle.GetGpuPtr(), ImVec2(512, 512));
        ImGui::End();
    }
#endif
    mSunDirection.Normalize();
    if (mAutoFitShadow)
    {
        ShadowCamera basis;
        basis.SetLookDirection(mSunDirection, std::abs(mSunDirection.y) > .95f ? Vector3::FORWARD : Vector3::UP);
        auto inverse = basis.GetRotation().Conjugate();
        AxisAlignedBox bounds;
        bool any = false;
        for (auto e : mWorld->View<MeshComponent, TransformComponent>())
        {
            auto &mesh = mWorld->GetComponent<MeshComponent>(e);
            if (!mesh.IsRender() || !mesh.GetModel())
                continue;
            auto matrix = mWorld->GetComponent<TransformComponent>(e).GetMatrix() * mesh.GetTransform().GetMatrix();
            auto box = mesh.GetModel()->mBoundingBox;
            auto low = box.GetMin(), high = box.GetMax();
            for (int corner = 0; corner < 8; ++corner)
                bounds.AddPoint(inverse *
                                Math::TransformCoord(Vector3(corner & 1 ? high.x : low.x, corner & 2 ? high.y : low.y,
                                                             corner & 4 ? high.z : low.z),
                                                     matrix));
            any = true;
        }
        if (any)
        {
            mShadowCenter = basis.GetRotation() * bounds.GetCenter();
            mShadowDim = Math::Max(bounds.GetMax() - bounds.GetMin() + Vector3(4, 4, 8), Vector3(8, 8, 8));
        }
    }

    mShadowCamera.UpdateMatrix(mSunDirection, mShadowCenter, mShadowDim, (uint32_t)gShadowBuffer.GetWidth(),
                               (uint32_t)gShadowBuffer.GetHeight());

    mSkybox.SetIBLBias(mIBLBias);

    GlobalConstants globals;
    globals.ViewProjMatrix = mCamera->GetViewProjMatrix();
    globals.SunShadowMatrix = mShadowCamera.GetShadowMatrix();
    globals.CameraPos = mCamera->GetPosition();
    globals.SunDirection = mSunDirection;
    globals.SunIntensity = mSunColor.ToVector3() * mSunLightIntensity;
    globals.IBLFactor = mIBLFactor;
    globals.IBLRange = mSkybox.GetIBLRange();
    globals.IBLBias = mSkybox.GetIBLBias();
    globals.ShadowTexelSize[0] = 1.0f / gShadowBuffer.GetWidth();
    globals.ShadowTexelSize[1] = 1.0f / gShadowBuffer.GetHeight();
    globals.InvTileDim[0] = 1.0f / LightManager::LightGridDim;
    globals.InvTileDim[1] = 1.0f / LightManager::LightGridDim;
    globals.TileCount[0] = DivideByMultiple(gSceneColorBuffer.GetWidth(), LightManager::LightGridDim);
    globals.TileCount[1] = DivideByMultiple(gSceneColorBuffer.GetHeight(), LightManager::LightGridDim);

    gfxContext.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    gfxContext.ClearDepth(gSceneDepthBuffer);

    RenderQueue queue(RenderQueue::kDefault);
    // zPrePass
    {
        queue.SetCamera(*mCamera);
        queue.SetViewportAndScissor(viewport, scissor);
        queue.SetDepthStencilTarget(gSceneDepthBuffer);
        queue.AddRenderTarget(gSceneColorBuffer);

        RenderObjects(queue);

        queue.Sort();
        queue.RenderMeshes(RenderQueue::kZPass, gfxContext, globals);
    }

    SSAO::Render(gfxContext, *mCamera);
    if (!SSAO::EnableDebugDraw())
    {
        LightManager::FillLightGrid(gfxContext, *mCamera);

        // Shadow
        {
            RenderQueue shadowSorter(RenderQueue::kShadows);
            shadowSorter.SetCamera(mShadowCamera);
            shadowSorter.SetDepthStencilTarget(gShadowBuffer);

            if (mShadowsEnabled)
                RenderObjects(shadowSorter);

            shadowSorter.Sort();
            shadowSorter.RenderMeshes(RenderQueue::kZPass, gfxContext, globals);
        }

        gfxContext.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        gfxContext.ClearColor(gSceneColorBuffer);

        // Main
        {
            // SSAO compute writes this texture as a UAV; forward shading samples it.
            gfxContext.TransitionResource(gSSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            gfxContext.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
            gfxContext.SetRenderTarget(gSceneColorBuffer.GetRTV(), gSceneDepthBuffer.GetDSV_DepthReadOnly());
            gfxContext.SetViewportAndScissor(viewport, scissor);

            if (mEnableIBL)
                mSkybox.Render(gfxContext, mCamera, viewport, scissor);

            queue.RenderMeshes(RenderQueue::kOpaque, gfxContext, globals);
        }

        queue.RenderMeshes(RenderQueue::kTransparent, gfxContext, globals);
    }
}

void ForwardRenderingPass::RenderObjects(RenderQueue &queue)
{
    auto view = mWorld->View<MeshComponent>();

    for (auto entity : view)
    {
        auto &mesh = view.get<MeshComponent>(entity);
        if (!mesh.IsRender())
            continue;
        mesh.Render(queue);
    }
}
} // namespace AtomEngine
