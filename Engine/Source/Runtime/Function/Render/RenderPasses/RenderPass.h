#pragma once
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Function/Framework/ECS/World.h"
#include "../RenderQueue.h"
#include "../RenderGraph.h"

namespace AtomEngine
{
enum class RenderPassStage
{
    Custom,
    Forward,
    Postprocess,
    Overlay
};
class RenderPass
{
  public:
    virtual ~RenderPass() = default;

    virtual void Initialize() {};
    virtual void Shutdown() {};
    virtual void Update(GraphicsContext &Context, float deltaTime) {};
    virtual void Render(GraphicsContext &gfxContext) = 0;
    virtual RenderPassStage Stage() const
    {
        return RenderPassStage::Custom;
    }
    virtual const char *Name() const
    {
        return "Custom";
    }
    virtual std::vector<GraphUse> GraphResources() const
    {
        return {{mRTV, GraphAccess::ReadWrite, D3D12_RESOURCE_STATE_RENDER_TARGET},
                {mDSV, GraphAccess::ReadWrite, D3D12_RESOURCE_STATE_DEPTH_WRITE}};
    }

    void SetRenderTarget(ColorBuffer *rtv, DepthBuffer *dsv)
    {
        mRTV = rtv;
        mDSV = dsv;
    }
    void SetDepthStencilTarget(DepthBuffer *dsv)
    {
        mDSV = dsv;
    }
    void SetCamera(Camera *camera)
    {
        mCamera = camera;
    }
    void SetViewportAndScissor(const D3D12_VIEWPORT &viewport, const D3D12_RECT &scissor)
    {
        mViewport = viewport;
        mScissor = scissor;
    }
    void SetWorld(World &world)
    {
        mWorld = &world;
    }
    World *GetWorld() const
    {
        return mWorld;
    }

  protected:
    const Camera *mCamera = nullptr;
    ColorBuffer *mRTV = nullptr;
    DepthBuffer *mDSV = nullptr;
    D3D12_VIEWPORT mViewport{};
    D3D12_RECT mScissor{};
    World *mWorld = nullptr;
};
} // namespace AtomEngine
