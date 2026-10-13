#pragma once
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include "Runtime/Platform/DirectX12/Buffer/GpuBuffer.h"

namespace AtomEngine
{
class ColorBuffer;
class ComputeContext;
class PostEffect
{
  public:
    void Initialize();
    void Shutdown();
    void Render(ComputeContext &Context);
    void SetExposure(float exposure, bool automatic)
    {
        mExposure = exposure;
        mEnableAdaptation = automatic;
    }

  private:
    /*--------
     パラメータ
    ---------*/
    bool mEnableHDR = true;

    // トーンマッピングパラメータ
    float mExposure = 1.0f;        // 適応露出が無効になっている場合の明るさスケール
    bool mEnableAdaptation = true; // 知覚される輝度に基づいて明るさを自動調整します

    // Adapationパラメータ
    float mMinExposure = 0.0f;
    float mMaxExposure = 8.0f;
    float mTargetLuminance = 0.08f;
    float mAdaptationRate = 0.05f;

    // ブルームパラメータ
    bool mEnableBloom = true;
    float mBloomThreshold = 4.0f;
    float mBloomStrength = 0.1f;
    float mBloomUpsampleFactor = 0.65f;

    float mHDRPaperWhite = 200.0f;
    float mMaxDisplayLuminance = 1000.0f;

  private:
    /*--------
     buffer
    ---------*/
    RootSignature mPostEffectsRS;
    ComputePSO mToneMapCS;
    ComputePSO mToneMapHDRCS;
    ComputePSO mApplyBloomCS;
    ComputePSO mDebugLuminanceHdrCS;
    ComputePSO mDebugLuminanceLdrCS;
    ComputePSO mGenerateHistogramCS;
    ComputePSO mAdaptExposureCS;
    ComputePSO mDownsampleBloomCS;
    ComputePSO mUpsampleAndBlurCS;
    ComputePSO mBlurCS;
    ComputePSO mBloomExtractAndDownsampleHdrCS;
    ComputePSO mBloomExtractAndDownsampleLdrCS;
    ComputePSO mExtractLumaCS;
    ComputePSO mCopyBackPostBufferCS;

  private:
    void UpdateExposure(ComputeContext &);
    void BlurBuffer(ComputeContext &, ColorBuffer buffer[2], const ColorBuffer &lowerResBuf, float upsampleBlendFactor);
    void GenerateBloom(ComputeContext &);
    void ExtractLuma(ComputeContext &);
    void ProcessHDR(ComputeContext &);
    void ProcessLDR(CommandContext &);
    void CopyBackPostBuffer(ComputeContext &Context);
};

} // namespace AtomEngine
