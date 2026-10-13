#include "RenderPassManager.h"
#include "../RenderGraph.h"
#include "../PostEffect/FXAA.h"

namespace AtomEngine
{
RenderPassManager *RenderPassManager::GetInstance()
{
    static RenderPassManager instance;
    return &instance;
}

void RenderPassManager::RegisterRenderPass(std::unique_ptr<RenderPass> pass)
{
    pass->Initialize();
    mAllPasses.push_back(std::move(pass));
}

void RenderPassManager::SetUpRenderPass()
{
    mCurrentPasses.clear();
    for (auto &pass : mAllPasses)
        mCurrentPasses.push_back(pass.get());
}

void RenderPassManager::Update(GraphicsContext &context, float deltaTime)
{
    for (auto *pass : mCurrentPasses)
        pass->Update(context, deltaTime);
}

void RenderPassManager::RenderAll(GraphicsContext &context)
{
    RenderGraph graph;
    for (GpuResource *r :
         std::initializer_list<GpuResource *>{&gSceneColorBuffer, &gSceneDepthBuffer, &gOverlayBuffer, &gFxaaBuffer})
        graph.Import(*r, false);
    for (auto pass : mCurrentPasses)
    {
        auto uses = pass->GraphResources();
        for (const auto &use : uses)
            if (use.resource)
                graph.Import(*use.resource);
        graph.AddPass(pass->Name(), std::move(uses), [pass](GraphicsContext &ctx) { pass->Render(ctx); });
        if (pass->Stage() == RenderPassStage::Postprocess && pass->GetWorld()->settings.fxaa)
        {
            graph.AddPass("FXAA",
                          {{&gSceneColorBuffer, GraphAccess::Read, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE},
                           {&gFxaaBuffer, GraphAccess::Write, D3D12_RESOURCE_STATE_RENDER_TARGET}},
                          [](GraphicsContext &ctx) { FXAA::Render(ctx); });
            graph.AddPass("FXAA resolve",
                          {{&gFxaaBuffer, GraphAccess::Read, D3D12_RESOURCE_STATE_COPY_SOURCE},
                           {&gSceneColorBuffer, GraphAccess::Write, D3D12_RESOURCE_STATE_COPY_DEST}},
                          [](GraphicsContext &ctx) { ctx.CopyBuffer(gSceneColorBuffer, gFxaaBuffer); });
        }
    }
    graph.Execute(context);
}

void RenderPassManager::SetRenderTarget(ColorBuffer *rtv, DepthBuffer *dsv)
{
    for (auto *pass : mCurrentPasses)
        pass->SetRenderTarget(rtv, dsv);
}

void RenderPassManager::SetDepthStencilTarget(DepthBuffer *dsv)
{
    for (auto *pass : mCurrentPasses)
        pass->SetDepthStencilTarget(dsv);
}

void RenderPassManager::SetCamera(Camera *camera)
{
    for (auto *pass : mCurrentPasses)
    {
        pass->SetCamera(camera);
    }
}

void RenderPassManager::SetViewportAndScissor(const D3D12_VIEWPORT &viewport, const D3D12_RECT &scissor)
{
    for (auto *pass : mCurrentPasses)
        pass->SetViewportAndScissor(viewport, scissor);
}

void RenderPassManager::SetWorld(World &world)
{
    for (auto *pass : mCurrentPasses)
        pass->SetWorld(world);
}

void RenderPassManager::Shutdown()
{
    for (auto it = mAllPasses.rbegin(); it != mAllPasses.rend(); ++it)
        (*it)->Shutdown();
    mAllPasses.clear();
    mCurrentPasses.clear();
}

} // namespace AtomEngine
