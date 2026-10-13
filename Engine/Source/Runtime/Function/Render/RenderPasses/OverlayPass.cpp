#include "OverlayPass.h"
#include "Runtime/Function/UI/UiRenderer.h"
#include "../SpriteRenderer.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"

namespace AtomEngine
{

std::vector<GraphUse> OverlayPass::GraphResources() const
{
    return {{&gOverlayBuffer, GraphAccess::Write, D3D12_RESOURCE_STATE_RENDER_TARGET}};
}
void OverlayPass::Render(GraphicsContext &gfxContext)
{
    GraphicsContext &UiContext = gfxContext;

    UiContext.TransitionResource(gOverlayBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    UiContext.ClearColor(gOverlayBuffer);
    UiContext.SetRenderTarget(gOverlayBuffer.GetRTV());
    UiContext.SetViewportAndScissor(0, 0, gOverlayBuffer.GetWidth(), gOverlayBuffer.GetHeight());

    SpriteRenderer::Render(UiContext);
    if (mWorld)
        UiRenderer::Render(UiContext, *mWorld);
}
} // namespace AtomEngine
