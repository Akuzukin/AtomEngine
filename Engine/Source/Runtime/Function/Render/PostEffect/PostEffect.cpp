#include "PostEffect.h"
#include "SSAO.h"

#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Core/GraphicsCommon.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Context/ComputeContext.h"
#include "imgui.h"
#include "Runtime/Function/Global/GlobalContext.h"

namespace
{
constexpr float kInitialMinLog = -12.0f;
constexpr float kInitialMaxLog = 4.0f;
} // namespace

namespace AtomEngine
{
void PostEffect::Initialize()
{
    mPostEffectsRS.Reset(4, 2);
    mPostEffectsRS.InitStaticSampler(0, SamplerLinearClampDesc);
    mPostEffectsRS.InitStaticSampler(1, SamplerLinearBorderDesc);
    mPostEffectsRS[0].InitAsConstants(0, 5);
    mPostEffectsRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
    mPostEffectsRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
    mPostEffectsRS[3].InitAsConstantBuffer(1);
    mPostEffectsRS.Finalize(L"Post Effects");

    auto toneMapCS = ShaderCompiler::CompileBlob(L"PostEffects/ToneMap.hlsl", L"cs_6_6");
    auto toneMap2CS = ShaderCompiler::CompileBlob(L"PostEffects/ToneMap2.hlsl", L"cs_6_6");
    auto toneMapHDRCS = ShaderCompiler::CompileBlob(L"PostEffects/ToneMapHDR.hlsl", L"cs_6_6");
    auto toneMapHDR2CS = ShaderCompiler::CompileBlob(L"PostEffects/ToneMapHDR2.hlsl", L"cs_6_6");

    auto applyBloomCS = ShaderCompiler::CompileBlob(L"PostEffects/ApplyBloom.hlsl", L"cs_6_6");
    auto applyBloom2CS = ShaderCompiler::CompileBlob(L"PostEffects/ApplyBloom2.hlsl", L"cs_6_6");

    auto debugLuminanceHdr = ShaderCompiler::CompileBlob(L"PostEffects/DebugLuminanceHdr.hlsl", L"cs_6_6");
    auto debugLuminanceHdr2 = ShaderCompiler::CompileBlob(L"PostEffects/DebugLuminanceHdr2.hlsl", L"cs_6_6");
    auto debugLuminanceLdr = ShaderCompiler::CompileBlob(L"PostEffects/DebugLuminanceLdr.hlsl", L"cs_6_6");
    auto debugLuminanceLdr2 = ShaderCompiler::CompileBlob(L"PostEffects/DebugLuminanceLdr2.hlsl", L"cs_6_6");

    auto generateHistogramCS = ShaderCompiler::CompileBlob(L"PostEffects/GenerateHistogram.hlsl", L"cs_6_6");
    auto adaptExposureCS = ShaderCompiler::CompileBlob(L"PostEffects/AdaptExposure.hlsl", L"cs_6_6");
    auto downsampleBloomCS = ShaderCompiler::CompileBlob(L"PostEffects/DownsampleBloom.hlsl", L"cs_6_6");
    auto upsampleAndBlurCS = ShaderCompiler::CompileBlob(L"PostEffects/UpsampleAndBlur.hlsl", L"cs_6_6");
    auto blurCS = ShaderCompiler::CompileBlob(L"PostEffects/Blur.hlsl", L"cs_6_6");
    auto bloomExtractAndDownsampleLdrCS =
        ShaderCompiler::CompileBlob(L"PostEffects/BloomExtractAndDownsampleLdr.hlsl", L"cs_6_6");
    auto bloomExtractAndDownsampleHdrCS =
        ShaderCompiler::CompileBlob(L"PostEffects/BloomExtractAndDownsampleHdr.hlsl", L"cs_6_6");
    auto extractLumaCS = ShaderCompiler::CompileBlob(L"PostEffects/ExtractLuma.hlsl", L"cs_6_6");
    auto copyBackPostBufferCS = ShaderCompiler::CompileBlob(L"PostEffects/CopyBackPostBuffer.hlsl", L"cs_6_6");
#define CreatePSO(ObjName, Shader)                                                                                     \
    ObjName.SetRootSignature(mPostEffectsRS);                                                                          \
    ObjName.SetComputeShader(Shader.Get());                                                                            \
    ObjName.Finalize();

    if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
    {
        CreatePSO(mToneMapCS, toneMap2CS);
        CreatePSO(mToneMapHDRCS, toneMapHDR2CS);
        CreatePSO(mApplyBloomCS, applyBloom2CS);
        CreatePSO(mDebugLuminanceHdrCS, debugLuminanceHdr2);
        CreatePSO(mDebugLuminanceLdrCS, debugLuminanceLdr2);
    }
    else
    {
        CreatePSO(mToneMapCS, toneMapCS);
        CreatePSO(mToneMapHDRCS, toneMapHDRCS);
        CreatePSO(mApplyBloomCS, applyBloomCS);
        CreatePSO(mDebugLuminanceHdrCS, debugLuminanceHdr);
        CreatePSO(mDebugLuminanceLdrCS, debugLuminanceLdr);
    }

    CreatePSO(mGenerateHistogramCS, generateHistogramCS);
    CreatePSO(mAdaptExposureCS, adaptExposureCS);
    CreatePSO(mDownsampleBloomCS, downsampleBloomCS);
    CreatePSO(mUpsampleAndBlurCS, upsampleAndBlurCS);
    CreatePSO(mBlurCS, blurCS);
    CreatePSO(mBloomExtractAndDownsampleHdrCS, bloomExtractAndDownsampleHdrCS);
    CreatePSO(mBloomExtractAndDownsampleLdrCS, bloomExtractAndDownsampleLdrCS);
    CreatePSO(mExtractLumaCS, extractLumaCS);
    CreatePSO(mCopyBackPostBufferCS, copyBackPostBufferCS);

#undef CreatePSO

    __declspec(align(16)) float initExposure[] = {mExposure,
                                                  1.0f / mExposure,
                                                  mExposure,
                                                  0.0f,
                                                  kInitialMinLog,
                                                  kInitialMaxLog,
                                                  kInitialMaxLog - kInitialMinLog,
                                                  1.0f / (kInitialMaxLog - kInitialMinLog)};

    // mMinExposure = 0.0f;
    // mMaxExposure = 1.0f;
    // mTargetLuminance = 0.45f;
    // mAdaptationRate = 0.5f;
    // mExposure = 1.0f;
    // mBloomThreshold = 0.8f;
    // mBloomStrength = 1.5f;
    // mBloomUpsampleFactor = 0.65f;
}

void PostEffect::Shutdown()
{
}

void PostEffect::Render(ComputeContext &Context)
{
#if !defined(RELEASE) && !defined(ATOM_GAME_RUNTIME)

    if (ImGui::GetCurrentContext() && gContext.showRenderDiagnostics)
    {
        ImGui::Begin("Post Effect");
        ImGui::Checkbox("EnableHDR", &mEnableHDR);
        ImGui::Checkbox("EnableAdaptation", &mEnableAdaptation);
        ImGui::DragFloat("MinExposure", &mMinExposure, 0.01f, -8.0f, 0.0f);
        ImGui::DragFloat("MaxExposure", &mMaxExposure, 0.01f, 0.0f, 8.0f);
        ImGui::DragFloat("TargetLuminance", &mTargetLuminance, 0.01f, 0.01f, 0.99f);
        ImGui::DragFloat("AdatationRate", &mAdaptationRate, 0.01f, 0.01f, 1.0f);
        ImGui::DragFloat("Exposure", &mExposure, 0.01f, -8.0f, 8.0f);

        ImGui::Separator();

        ImGui::Checkbox("EnableBloom", &mEnableBloom);
        ImGui::DragFloat("BloomThreshold", &mBloomThreshold, 0.01f, 0.0f, 8.0f);
        ImGui::DragFloat("BloomStrength", &mBloomStrength, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("BloomUpsampleFactor", &mBloomUpsampleFactor, 0.01f, 0.0f, 1.0f);
        ImGui::End();
    }
#endif
    Context.SetRootSignature(mPostEffectsRS);

    Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if (mEnableHDR && !SSAO::EnableDebugDraw() /*&& !(DepthOfField::Enable && DepthOfField::DebugMode >= 3)*/)
        ProcessHDR(Context);
    else
        ProcessLDR(Context);

    bool bGeneratedLumaBuffer = mEnableHDR /*|| FXAA::DebugDraw || BloomEnable*/;
    // if (FXAA::Enable)
    //	FXAA::Render(Context, bGeneratedLumaBuffer);

    // 別のバッファでポストプロセス処理を行っていた場合は、それを元のバッファにコピーする必要があります。
    // 次のシェーダーが UINT
    // から手動でフォーマットデコードを行うことが分かっている場合は、この手順を省略できますが、変更が必要なコードパスがいくつかあり、
    // そのうちのいくつかはテクスチャフィルタリングに依存しており、UINT では機能しません。
    // これはレガシーハードウェアをサポートするためのものであり、単一のバッファコピーはそれほど大きな問題ではないため、
    // 最も経済的な解決策です。
    if (!DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
        CopyBackPostBuffer(Context);
}

void PostEffect::GenerateBloom(ComputeContext &Context)
{
    uint32_t kBloomWidth = gLumaLR.GetWidth();
    uint32_t kBloomHeight = gLumaLR.GetHeight();

    // これらのブルームバッファのサイズは、128で割り切れるという優れた数値と、およそ16:9であることから選択されました。
    // ぼかしアルゴリズムは9ピクセル×9ピクセルなので、各ピクセルのアスペクト比が正方形でない場合、ぼかしは
    // 円形ではなく楕円形になります。偶然にも、これらは720pバッファの1/2、1080pバッファの1/3にほぼ相当します。
    // これは、コンソール上のブルームバッファの一般的なサイズです。
    ASSERT(kBloomWidth % 16 == 0 && kBloomHeight % 16 == 0, "Bloom buffer dimensions must be multiples of 16");

    Context.SetConstants(0, 1.0f / kBloomWidth, 1.0f / kBloomHeight, mBloomThreshold);
    Context.TransitionResource(gBloomUAV1[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gLumaLR, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gViewExposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    {
        Context.SetDynamicDescriptor(1, 0, gBloomUAV1[0].GetUAV());
        Context.SetDynamicDescriptor(1, 1, gLumaLR.GetUAV());
        Context.SetDynamicDescriptor(2, 0, gSceneColorBuffer.GetSRV());
        Context.SetDynamicDescriptor(2, 1, gViewExposure.GetSRV());

        Context.SetPipelineState(mEnableHDR ? mBloomExtractAndDownsampleHdrCS : mBloomExtractAndDownsampleLdrCS);
        Context.Dispatch2D(kBloomWidth, kBloomHeight);
    }

    Context.TransitionResource(gBloomUAV1[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.SetDynamicDescriptor(2, 0, gBloomUAV1[0].GetSRV());

    Context.TransitionResource(gBloomUAV2[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gBloomUAV3[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gBloomUAV4[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gBloomUAV5[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    D3D12_CPU_DESCRIPTOR_HANDLE UAVs[4] = {gBloomUAV2[0].GetUAV(), gBloomUAV3[0].GetUAV(), gBloomUAV4[0].GetUAV(),
                                           gBloomUAV5[0].GetUAV()};
    Context.SetDynamicDescriptors(1, 0, 4, UAVs);

    // 各ディスパッチ グループは 8x8 スレッドですが、各スレッドは 2x2 ソーステクセルを読み取ります。
    Context.SetPipelineState(mDownsampleBloomCS);
    Context.Dispatch2D(kBloomWidth / 2, kBloomHeight / 2);

    Context.TransitionResource(gBloomUAV2[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gBloomUAV3[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gBloomUAV4[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gBloomUAV5[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // ぼかし、アップサンプリング、そして4回ぼかしをかける
    BlurBuffer(Context, gBloomUAV5, gBloomUAV5[0], 1.0f);
    BlurBuffer(Context, gBloomUAV4, gBloomUAV5[1], mBloomUpsampleFactor);
    BlurBuffer(Context, gBloomUAV3, gBloomUAV4[1], mBloomUpsampleFactor);
    BlurBuffer(Context, gBloomUAV2, gBloomUAV3[1], mBloomUpsampleFactor);
    BlurBuffer(Context, gBloomUAV1, gBloomUAV2[1], mBloomUpsampleFactor);
}

void PostEffect::ExtractLuma(ComputeContext &Context)
{
    Context.TransitionResource(gLumaLR, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gViewExposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.SetConstants(0, 1.0f / gLumaLR.GetWidth(), 1.0f / gLumaLR.GetHeight());
    Context.SetDynamicDescriptor(1, 0, gLumaLR.GetUAV());
    Context.SetDynamicDescriptor(2, 0, gSceneColorBuffer.GetSRV());
    Context.SetDynamicDescriptor(2, 1, gViewExposure.GetSRV());
    Context.SetPipelineState(mExtractLumaCS);
    Context.Dispatch2D(gLumaLR.GetWidth(), gLumaLR.GetHeight());
}

void PostEffect::ProcessHDR(ComputeContext &Context)
{
    if (mEnableBloom)
    {
        GenerateBloom(Context);
        Context.TransitionResource(gBloomUAV1[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    else if (mEnableAdaptation)
        ExtractLuma(Context);

    if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
        Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    else
        Context.TransitionResource(gPostEffectsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    Context.TransitionResource(gLumaBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gViewExposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    Context.SetPipelineState(DX12Core::gEnableHDROutput ? mToneMapHDRCS : mToneMapCS);

    // Set constants
    Context.SetConstants(0, 1.0f / gSceneColorBuffer.GetWidth(), 1.0f / gSceneColorBuffer.GetHeight(), mBloomStrength);
    Context.SetConstant(0, 3, mHDRPaperWhite / mMaxDisplayLuminance);
    Context.SetConstant(0, 4, mMaxDisplayLuminance);

    // SDRの結果を知覚される輝度から分離する
    if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
        Context.SetDynamicDescriptor(1, 0, gSceneColorBuffer.GetUAV());
    else
    {
        Context.SetDynamicDescriptor(1, 0, gPostEffectsBuffer.GetUAV());
        Context.SetDynamicDescriptor(2, 2, gSceneColorBuffer.GetSRV());
    }
    Context.SetDynamicDescriptor(1, 1, gLumaBuffer.GetUAV());

    // オリジナルのHDR値とぼかしブルームバッファを読み込む
    Context.SetDynamicDescriptor(2, 0, gViewExposure.GetSRV());
    Context.SetDynamicDescriptor(2, 1, mEnableBloom ? gBloomUAV1[1].GetSRV() : GetDefaultTexture(kBlackOpaque2D));

    Context.Dispatch2D(gSceneColorBuffer.GetWidth(), gSceneColorBuffer.GetHeight());

    // これを最後に行うと、明るいパスはトーンマッピングと同じ露出を使用します。
    UpdateExposure(Context);
}

void PostEffect::ProcessLDR(CommandContext &BaseContext)
{
    ComputeContext &Context = BaseContext.GetComputeContext();

    bool bGenerateBloom = mEnableBloom && !SSAO::EnableDebugDraw();
    if (bGenerateBloom)
        GenerateBloom(Context);

    if (bGenerateBloom || /*FXAA::DebugDraw || */ SSAO::EnableDebugDraw() ||
        !DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
    {
        if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
            Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        else
            Context.TransitionResource(gPostEffectsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        Context.TransitionResource(gBloomUAV1[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gLumaBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // Set constants
        Context.SetConstants(0, 1.0f / gSceneColorBuffer.GetWidth(), 1.0f / gSceneColorBuffer.GetHeight(),
                             mBloomStrength);

        // SDR結果をその知覚輝度から分離する
        if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
            Context.SetDynamicDescriptor(1, 0, gSceneColorBuffer.GetUAV());
        else
        {
            Context.SetDynamicDescriptor(1, 0, gPostEffectsBuffer.GetUAV());
            Context.SetDynamicDescriptor(2, 2, gSceneColorBuffer.GetSRV());

            Context.SetDynamicDescriptor(1, 1, gLumaBuffer.GetUAV());

            // 元のSDR値とぼかしブルームバッファを読み込む
            Context.SetDynamicDescriptor(2, 0,
                                         bGenerateBloom ? gBloomUAV1[1].GetSRV() : GetDefaultTexture(kBlackOpaque2D));

            Context.SetPipelineState(/*FXAA::DebugDraw ? mDebugLuminanceLdrCS : */ mApplyBloomCS);
            Context.Dispatch2D(gSceneColorBuffer.GetWidth(), gSceneColorBuffer.GetHeight());

            Context.TransitionResource(gLumaBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
    }
}
void PostEffect::UpdateExposure(ComputeContext &Context)
{
    if (!mEnableAdaptation)
    {
        __declspec(align(16)) float initExposure[] = {mExposure,
                                                      1.0f / mExposure,
                                                      mExposure,
                                                      0.0f,
                                                      kInitialMinLog,
                                                      kInitialMaxLog,
                                                      kInitialMaxLog - kInitialMinLog,
                                                      1.0f / (kInitialMaxLog - kInitialMinLog)};
        Context.WriteBuffer(gViewExposure, 0, initExposure, sizeof(initExposure));
        Context.TransitionResource(gViewExposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        return;
    }

    // HDRヒストグラムを作成
    Context.TransitionResource(gHistogram, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    Context.ClearUAV(gHistogram);
    Context.TransitionResource(gLumaLR, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.SetDynamicDescriptor(1, 0, gHistogram.GetUAV());
    Context.SetDynamicDescriptor(2, 0, gLumaLR.GetSRV());
    Context.SetPipelineState(mGenerateHistogramCS);
    Context.SetConstants(0, gLumaLR.GetHeight());
    Context.Dispatch2D(gLumaLR.GetWidth(), gLumaLR.GetHeight(), 16, gLumaLR.GetHeight());

    __declspec(align(16)) struct
    {
        float TargetLuminance;
        float AdaptationRate;
        float MinExposure;
        float MaxExposure;
        uint32_t PixelCount;
    } constants = {mTargetLuminance, mAdaptationRate, mMinExposure, mMaxExposure,
                   gLumaLR.GetWidth() * gLumaLR.GetHeight()};
    Context.TransitionResource(gHistogram, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gViewExposure, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.SetDynamicDescriptor(1, 0, gViewExposure.GetUAV());
    Context.SetDynamicDescriptor(2, 0, gHistogram.GetSRV());
    Context.SetDynamicConstantBufferView(3, sizeof(constants), &constants);
    Context.SetPipelineState(mAdaptExposureCS);
    Context.Dispatch();
    Context.TransitionResource(gViewExposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
void PostEffect::BlurBuffer(ComputeContext &Context, ColorBuffer buffer[2], const ColorBuffer &lowerResBuf,
                            float upsampleBlendFactor)
{
    uint32_t bufferWidth = buffer[0].GetWidth();
    uint32_t bufferHeight = buffer[0].GetHeight();
    Context.SetConstants(0, 1.0f / bufferWidth, 1.0f / bufferHeight, upsampleBlendFactor);

    Context.TransitionResource(buffer[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.SetDynamicDescriptor(1, 0, buffer[1].GetUAV());
    D3D12_CPU_DESCRIPTOR_HANDLE SRVs[2] = {buffer[0].GetSRV(), lowerResBuf.GetSRV()};
    Context.SetDynamicDescriptors(2, 0, 2, SRVs);

    Context.SetPipelineState(&buffer[0] == &lowerResBuf ? mBlurCS : mUpsampleAndBlurCS);

    Context.Dispatch2D(bufferWidth, bufferHeight);

    Context.TransitionResource(buffer[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void PostEffect::CopyBackPostBuffer(ComputeContext &Context)
{
    Context.SetRootSignature(mPostEffectsRS);
    Context.SetPipelineState(mCopyBackPostBufferCS);
    Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gPostEffectsBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.SetDynamicDescriptor(1, 0, gSceneColorBuffer.GetUAV());
    Context.SetDynamicDescriptor(2, 0, gPostEffectsBuffer.GetSRV());
    Context.Dispatch2D(gSceneColorBuffer.GetWidth(), gSceneColorBuffer.GetHeight());
}
} // namespace AtomEngine
