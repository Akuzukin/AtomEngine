#pragma once
#include "RenderPass.h"

namespace AtomEngine
{
class RenderPassManager
{
  public:
    RenderPassManager() = default;
    ~RenderPassManager() = default;
    static RenderPassManager *GetInstance();

    void RegisterRenderPass(std::unique_ptr<RenderPass> pass);
    void SetUpRenderPass();

    void Update(GraphicsContext &context, float deltaTime);
    void RenderAll(GraphicsContext &context);

    void SetRenderTarget(ColorBuffer *rtv, DepthBuffer *dsv);
    void SetDepthStencilTarget(DepthBuffer *dsv);
    void SetCamera(Camera *camera);
    void SetViewportAndScissor(const D3D12_VIEWPORT &viewport, const D3D12_RECT &scissor);
    void SetWorld(World &world);

    void Shutdown();

  private:
    std::vector<std::unique_ptr<RenderPass>> mAllPasses;
    std::vector<RenderPass *> mCurrentPasses;

  private:
    RenderPassManager(const RenderPassManager &) = delete;
    RenderPassManager &operator=(const RenderPassManager &) = delete;
};

} // namespace AtomEngine
