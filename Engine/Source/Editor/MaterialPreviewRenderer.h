#pragma once
#include "Runtime/Resource/Material.h"
#include "Runtime/Resource/TextureRef.h"
#include "Runtime/Resource/Model.h"
#include "Runtime/Platform/DirectX12/Buffer/ColorBuffer.h"
#include "Runtime/Platform/DirectX12/Buffer/DepthBuffer.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
namespace AtomEngine
{
class MaterialPreviewRenderer
{
  public:
    ~MaterialPreviewRenderer();
    void Initialize();
    void Render(const MaterialInstance &material, float yaw = 0, float pitch = .25f,
                std::shared_ptr<Model> source = nullptr, int slot = 0);
    uint64_t Texture() const
    {
        return mDisplay.GetGpuPtr();
    }
    ColorBuffer &Buffer()
    {
        return mColor;
    }

  private:
    ColorBuffer mColor;
    DepthBuffer mDepth;
    GraphicsPSO mPSO;
    GraphicsPSO mMRPSO;
    DescriptorHandle mDisplay, mTextures, mEnvironment;
    std::shared_ptr<Model> mSphere;
    std::shared_ptr<Model> mSource;
    std::vector<TextureRef> mTextureRefs, mEnvironmentRefs;
    std::string mTextureSignature;
};
} // namespace AtomEngine
