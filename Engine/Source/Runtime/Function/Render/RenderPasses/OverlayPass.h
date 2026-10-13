#pragma once
#include "RenderPass.h"

namespace AtomEngine
{
class OverlayPass : public RenderPass
{
  public:
    void Render(GraphicsContext &gfxContext) override;
    RenderPassStage Stage() const override
    {
        return RenderPassStage::Overlay;
    }
    const char *Name() const override
    {
        return "Overlay";
    }
    std::vector<GraphUse> GraphResources() const override;
};
} // namespace AtomEngine
