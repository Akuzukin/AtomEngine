#pragma once
#include "RenderPass.h"
#include "Runtime/Function/Camera/ShadowCamera.h"

#include "../SkyboxRenderer.h"

namespace AtomEngine
{
class ForwardRenderingPass : public RenderPass
{
  public:
    ForwardRenderingPass();
    ~ForwardRenderingPass();

    void Render(GraphicsContext &gfxContext) override;

  private:
    SkyboxRenderer mSkybox;
    ShadowCamera mShadowCamera;

    Color mSunColor = Color::White;
    float mSunLightIntensity = 1.0f;
    Vector3 mSunDirection = {.35f, -1, .25f};
    Vector3 mShadowCenter = {0.0f, -500.0f, 0.0f};
    Vector3 mShadowDim = {5000, 3000, 3000};
    float mIBLBias = 2.0f;
    float mIBLFactor = 1.0f;
    bool mEnableIBL = true;
    bool mAutoFitShadow = true;
    bool mShadowsEnabled = true;
    DescriptorHandle mShadowSrvGpuHandle;

  private:
    void RenderObjects(RenderQueue &queue);
};
} // namespace AtomEngine
