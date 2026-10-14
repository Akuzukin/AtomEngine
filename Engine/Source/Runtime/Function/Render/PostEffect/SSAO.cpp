#include "SSAO.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Platform/DirectX12/Context/ComputeContext.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "imgui.h"
#include "Runtime/Function/Global/GlobalContext.h"

// param
namespace
{
enum QualityLevel
{
    kSsaoQualityVeryLow,
    kSsaoQualityLow,
    kSsaoQualityMedium,
    kSsaoQualityHigh,
    kSsaoQualityVeryHigh,
    kNumSsaoQualitySettings
};
const char *QualityLabels[kNumSsaoQualitySettings] = {"Very Low", "Low", "Medium", "High", "Very High"};
QualityLevel gQualityLevel = kSsaoQualityHigh;
} // namespace

namespace
{
bool sEnableSSAO = true;
bool sEnableDebugView = false;

// これは、解像度が大幅に低下したバイラテラルアップサンプリングによるピクセルの揺らめきを除去するために必要です。
// 高周波ディテールは再現できない場合があり、ノイズフィルターは、高解像度のSSAOの結果で欠落したピクセルを補います。
float sNoiseFilterTolerance = -3.0f;
float sBlurTolerance = -5.0f;
float sUpsampleTolerance = -7.0f;

// 球体を遮蔽するサンプルをどの程度フェードオフするかを制御しますが、あまりにフェードオフが強すぎると信頼性が低下します。
// これは、壁の前に置かれたオブジェクトの周りに暗いハローを与えるものです。ハローをフェードオフしたい場合は、
// 除去減衰を大きくします。ただし、全体的なAOは低下します。
float sRejectionFalloff = 2.5f;

// このエフェクトは通常、50%以下の遮蔽率を持つものを「完全に遮蔽されていない」とマークします。これにより、
// 結果の半分が無駄になります。アクセントはエフェクトの範囲を広げますが、処理中にすべてのAO値が暗くなります。
// また、「遮蔽不足」のジオメトリがハイライト表示されるようになります。環境光が
// 面法線によって決定される場合（IBLなど）、この副作用は望ましくないかもしれません。
float sAccentuation = 0.1f;

int sHierarchyDepth = 3;

AtomEngine::RootSignature sRootSignature;
AtomEngine::ComputePSO sDepthPrepare1CS = L"SSAO: Depth Prepare 1 CS";
AtomEngine::ComputePSO sDepthPrepare2CS = L"SSAO: Depth Prepare 2 CS";
AtomEngine::ComputePSO sRender1CS = L"SSAO: Render 1 CS";
AtomEngine::ComputePSO sRender2CS = L"SSAO: Render 2 CS";
AtomEngine::ComputePSO sBlurUpsampleBlend[2] = {
    {L"SSAO: Blur Upsample Blend Low Q. CS"},
    {L"SSAO: Blur Upsample Blend High Q. CS"}}; // Blend the upsampled result with the next higher resolution
AtomEngine::ComputePSO sBlurUpsampleFinal[2] = {
    {L"SSAO: Blur Upsample Final Low Q. CS"},
    {L"SSAO: Blur Upsample Final High Q. CS"}}; // Don't blend the result, just upsample it
AtomEngine::ComputePSO sLinearizeDepthCS = L"SSAO: Linearize Depth CS";
AtomEngine::ComputePSO sDebugSSAOCS = L"SSAO: Debug CS";

float SampleThickness[12]; // Pre-computed sample thicknesses

} // namespace
namespace AtomEngine
{
void SSAO::Initialize()
{
    sRootSignature.Reset(5, 2);
    sRootSignature.InitStaticSampler(0, SamplerLinearClampDesc);
    sRootSignature.InitStaticSampler(1, SamplerLinearBorderDesc);
    sRootSignature[0].InitAsConstants(0, 4);
    sRootSignature[1].InitAsConstantBuffer(1);
    sRootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 5);
    sRootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
    sRootSignature[4].InitAsBufferSRV(5);
    sRootSignature.Finalize(L"SSAO");

    auto aoPrepareDepthBuffers1CS =
        ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoPrepareDepthBuffers1.hlsl", L"cs_6_6");
    auto aoPrepareDepthBuffers2CS =
        ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoPrepareDepthBuffers2.hlsl", L"cs_6_6");

    auto linearizeDepthCS = ShaderCompiler::CompileBlob(L"PostEffects/SSAO/LinearizeDepth.hlsl", L"cs_6_6");

    auto debugSSAOCS = ShaderCompiler::CompileBlob(L"PostEffects/SSAO/DebugSSAO.hlsl", L"cs_6_6");
    auto aoRender1CS = ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoRender1.hlsl", L"cs_6_6");
    auto aoRender2CS = ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoRender2.hlsl", L"cs_6_6");

    auto aoBlurUpsampleBlendOutCS =
        ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoBlurUpsampleBlendOut.hlsl", L"cs_6_6");
    auto aoBlurUpsamplePreMinBlendOutCS =
        ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoBlurUpsamplePreMinBlendOut.hlsl", L"cs_6_6");
    auto aoBlurUpsampleCS = ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoBlurUpsample.hlsl", L"cs_6_6");
    auto aoBlurUpsamplePreMinCS = ShaderCompiler::CompileBlob(L"PostEffects/SSAO/AoBlurUpsamplePreMin.hlsl", L"cs_6_6");

#define CreatePSO(ObjName, Shader)                                                                                     \
    ObjName.SetRootSignature(sRootSignature);                                                                          \
    ObjName.SetComputeShader(Shader.Get());                                                                            \
    ObjName.Finalize();

    CreatePSO(sDepthPrepare1CS, aoPrepareDepthBuffers1CS);
    CreatePSO(sDepthPrepare2CS, aoPrepareDepthBuffers2CS);
    CreatePSO(sLinearizeDepthCS, linearizeDepthCS);
    CreatePSO(sDebugSSAOCS, debugSSAOCS);
    CreatePSO(sRender1CS, aoRender1CS);
    CreatePSO(sRender2CS, aoRender2CS);

    CreatePSO(sBlurUpsampleBlend[0], aoBlurUpsampleBlendOutCS);
    CreatePSO(sBlurUpsampleBlend[1], aoBlurUpsamplePreMinBlendOutCS);
    CreatePSO(sBlurUpsampleFinal[0], aoBlurUpsampleCS);
    CreatePSO(sBlurUpsampleFinal[1], aoBlurUpsamplePreMinCS);

    SampleThickness[0] = sqrtf(1.0f - 0.2f * 0.2f);
    SampleThickness[1] = sqrtf(1.0f - 0.4f * 0.4f);
    SampleThickness[2] = sqrtf(1.0f - 0.6f * 0.6f);
    SampleThickness[3] = sqrtf(1.0f - 0.8f * 0.8f);
    SampleThickness[4] = sqrtf(1.0f - 0.2f * 0.2f - 0.2f * 0.2f);
    SampleThickness[5] = sqrtf(1.0f - 0.2f * 0.2f - 0.4f * 0.4f);
    SampleThickness[6] = sqrtf(1.0f - 0.2f * 0.2f - 0.6f * 0.6f);
    SampleThickness[7] = sqrtf(1.0f - 0.2f * 0.2f - 0.8f * 0.8f);
    SampleThickness[8] = sqrtf(1.0f - 0.4f * 0.4f - 0.4f * 0.4f);
    SampleThickness[9] = sqrtf(1.0f - 0.4f * 0.4f - 0.6f * 0.6f);
    SampleThickness[10] = sqrtf(1.0f - 0.4f * 0.4f - 0.8f * 0.8f);
    SampleThickness[11] = sqrtf(1.0f - 0.6f * 0.6f - 0.6f * 0.6f);

    // sNoiseFilterTolerance = -6.0f;
    // sBlurTolerance = -5.0f;
    // sUpsampleTolerance = -7.5f;
    // sRejectionFalloff = 1.5f;
}
void SSAO::Shutdown()
{
}
void SSAO::Render(GraphicsContext &GfxContext, const float *ProjMat, float NearClipDist, float FarClipDist)
{
#if !defined(RELEASE) && !defined(ATOM_GAME_RUNTIME)
    if (ImGui::GetCurrentContext() && gContext.showRenderDiagnostics)
    {
        ImGui::Begin("SSAO");
        ImGui::Checkbox("Enable SSAO", &sEnableSSAO);
        ImGui::Checkbox("SSAO Debug View", &sEnableDebugView);
        ImGui::DragFloat("NoiseFilterTolerance(log10)", &sNoiseFilterTolerance, 0.25f, -8.0f, 0.0f);
        ImGui::DragFloat("BlurTolerance(log10)", &sBlurTolerance, 0.25f, -8.0f, -1.0f);
        ImGui::DragFloat("UpsampleTolerance(log10)", &sUpsampleTolerance, 0.5f, -12.0f, -1.0f);
        ImGui::DragFloat("RejectionFalloff", &sRejectionFalloff, 0.1f, 1.0f, 10.0f);
        ImGui::DragFloat("Accentuation", &sAccentuation, 0.01f, 0.0f, 1.0f);
        ImGui::DragInt("HierarchyDepth", &sHierarchyDepth, 1, 1, 4);
        ImGui::DragInt("QualityLevel", reinterpret_cast<int *>(&gQualityLevel), 1, 0, kNumSsaoQualitySettings - 1);
        ImGui::End();
    }
#endif // !RELEASE

    uint32_t FrameIndex = uint32_t(GetViewBuffers().frameIndex & 1);

    DepthBuffer &Depth = gSceneDepthBuffer;
    ColorBuffer &LinearDepth = gLinearDepth[FrameIndex];

    const float zMagic = (FarClipDist - NearClipDist) / NearClipDist;

    if (!sEnableSSAO)
    {
        // Clear the SSAO buffer
        GfxContext.TransitionResource(gSSAOFullScreen, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        GfxContext.ClearColor(gSSAOFullScreen);
        GfxContext.TransitionResource(gSSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        ComputeContext &Context = GfxContext.GetComputeContext();

        LinearizeZ(Context, Depth, LinearDepth, zMagic);

        if (sEnableDebugView)
        {
            Context.SetRootSignature(sRootSignature);
            Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            Context.SetDynamicDescriptors(2, 0, 1, &gSceneColorBuffer.GetUAV());
            Context.SetDynamicDescriptors(3, 0, 1, &LinearDepth.GetSRV());
            Context.SetPipelineState(sDebugSSAOCS);
            Context.Dispatch2D(gSSAOFullScreen.GetWidth(), gSSAOFullScreen.GetHeight());
        }

        return;
    }

    GfxContext.TransitionResource(Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    GfxContext.TransitionResource(gSSAOFullScreen, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    ComputeContext &Context = GfxContext.GetComputeContext();
    Context.SetRootSignature(sRootSignature);

    {
        // フェーズ 1: 深度バッファの解凍、線形化、ダウンサンプリング、デインターリーブ
        Context.SetConstants(0, zMagic);
        Context.SetDynamicDescriptor(3, 0, Depth.GetDepthSRV());
        Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gDepthDownsize1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gDepthTiled1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gDepthDownsize2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gDepthTiled2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        D3D12_CPU_DESCRIPTOR_HANDLE DownsizeUAVs[5] = {LinearDepth.GetUAV(), gDepthDownsize1.GetUAV(),
                                                       gDepthTiled1.GetUAV(), gDepthDownsize2.GetUAV(),
                                                       gDepthTiled2.GetUAV()};
        Context.SetDynamicDescriptors(2, 0, 5, DownsizeUAVs);

        Context.SetPipelineState(sDepthPrepare1CS);
        Context.Dispatch2D(gDepthTiled2.GetWidth() * 8, gDepthTiled2.GetHeight() * 8);

        if (sHierarchyDepth > 2)
        {
            Context.SetConstants(0, 1.0f / gDepthDownsize2.GetWidth(), 1.0f / gDepthDownsize2.GetHeight());
            Context.TransitionResource(gDepthDownsize2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            Context.TransitionResource(gDepthDownsize3, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Context.TransitionResource(gDepthTiled3, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Context.TransitionResource(gDepthDownsize4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Context.TransitionResource(gDepthTiled4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            D3D12_CPU_DESCRIPTOR_HANDLE DownsizeAgainUAVs[4] = {gDepthDownsize3.GetUAV(), gDepthTiled3.GetUAV(),
                                                                gDepthDownsize4.GetUAV(), gDepthTiled4.GetUAV()};
            Context.SetDynamicDescriptors(2, 0, 4, DownsizeAgainUAVs);
            Context.SetDynamicDescriptors(3, 0, 1, &gDepthDownsize2.GetSRV());
            Context.SetPipelineState(sDepthPrepare2CS);
            Context.Dispatch2D(gDepthTiled4.GetWidth() * 8, gDepthTiled4.GetHeight() * 8);
        }

    } // 解凍終了
    {
        // 水平 FOV のコタンジェントを 2 で割った投影行列の最初の要素を読み込みます。
        const float FovTangent = 1.0f / ProjMat[0];

        Context.TransitionResource(gAOMerged1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gAOMerged2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gAOMerged3, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gAOMerged4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gAOHighQuality1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gAOHighQuality2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gAOHighQuality3, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gAOHighQuality4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Context.TransitionResource(gDepthTiled1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gDepthTiled2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gDepthTiled3, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gDepthTiled4, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gDepthDownsize1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gDepthDownsize2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gDepthDownsize3, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.TransitionResource(gDepthDownsize4, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        // フェーズ2: 各サブタイルのSSAOをレンダリングする
        if (sHierarchyDepth > 3)
        {
            Context.SetPipelineState(sRender1CS);
            ComputeAO(Context, gAOMerged4, gDepthTiled4, FovTangent);
            if (gQualityLevel >= kSsaoQualityLow)
            {
                Context.SetPipelineState(sRender2CS);
                ComputeAO(Context, gAOHighQuality4, gDepthDownsize4, FovTangent);
            }
        }
        if (sHierarchyDepth > 2)
        {
            Context.SetPipelineState(sRender1CS);
            ComputeAO(Context, gAOMerged3, gDepthTiled3, FovTangent);
            if (gQualityLevel >= kSsaoQualityMedium)
            {
                Context.SetPipelineState(sRender2CS);
                ComputeAO(Context, gAOHighQuality3, gDepthDownsize3, FovTangent);
            }
        }
        if (sHierarchyDepth > 1)
        {
            Context.SetPipelineState(sRender1CS);
            ComputeAO(Context, gAOMerged2, gDepthTiled2, FovTangent);
            if (gQualityLevel >= kSsaoQualityHigh)
            {
                Context.SetPipelineState(sRender2CS);
                ComputeAO(Context, gAOHighQuality2, gDepthDownsize2, FovTangent);
            }
        }
        {
            Context.SetPipelineState(sRender1CS);
            ComputeAO(Context, gAOMerged1, gDepthTiled1, FovTangent);
            if (gQualityLevel >= kSsaoQualityVeryHigh)
            {
                Context.SetPipelineState(sRender2CS);
                ComputeAO(Context, gAOHighQuality1, gDepthDownsize1, FovTangent);
            }
        }

    } // 分析終了
    {
        // フェーズ4: 反復的にぼかしとアップサンプリングを行い、各結果を組み合わせる

        ColorBuffer *NextSRV = &gAOMerged4;

        // 120 x 68 -> 240 x 135
        if (sHierarchyDepth > 3)
        {
            BlurAndUpsample(Context, gAOSmooth3, gDepthDownsize3, gDepthDownsize4, NextSRV,
                            gQualityLevel >= kSsaoQualityLow ? &gAOHighQuality4 : nullptr, &gAOMerged3);

            NextSRV = &gAOSmooth3;
        }
        else
            NextSRV = &gAOMerged3;

        // 240 x 135 -> 480 x 270
        if (sHierarchyDepth > 2)
        {
            BlurAndUpsample(Context, gAOSmooth2, gDepthDownsize2, gDepthDownsize3, NextSRV,
                            gQualityLevel >= kSsaoQualityMedium ? &gAOHighQuality3 : nullptr, &gAOMerged2);

            NextSRV = &gAOSmooth2;
        }
        else
            NextSRV = &gAOMerged2;

        // 480 x 270 -> 960 x 540
        if (sHierarchyDepth > 1)
        {
            BlurAndUpsample(Context, gAOSmooth1, gDepthDownsize1, gDepthDownsize2, NextSRV,
                            gQualityLevel >= kSsaoQualityHigh ? &gAOHighQuality2 : nullptr, &gAOMerged1);

            NextSRV = &gAOSmooth1;
        }
        else
            NextSRV = &gAOMerged1;

        // 960 x 540 -> 1920 x 1080
        BlurAndUpsample(Context, gSSAOFullScreen, LinearDepth, gDepthDownsize1, NextSRV,
                        gQualityLevel >= kSsaoQualityVeryHigh ? &gAOHighQuality1 : nullptr, nullptr);

    } // ぼかしとアップサンプリングを終了

    if (sEnableDebugView)
    {

        ComputeContext &CC = GfxContext.GetComputeContext();
        CC.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        CC.TransitionResource(gSSAOFullScreen, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        CC.SetRootSignature(sRootSignature);
        CC.SetPipelineState(sDebugSSAOCS);
        CC.SetDynamicDescriptors(2, 0, 1, &gSceneColorBuffer.GetUAV());
        CC.SetDynamicDescriptors(3, 0, 1, &gSSAOFullScreen.GetSRV());
        CC.Dispatch2D(gSSAOFullScreen.GetWidth(), gSSAOFullScreen.GetHeight());
    }
}
void SSAO::Render(GraphicsContext &Context, const Camera &camera)
{
    const float *pProjMat = reinterpret_cast<const float *>(&camera.GetProjMatrix());
    Render(Context, pProjMat, camera.GetNearClip(), camera.GetFarClip());
}
void SSAO::LinearizeZ(ComputeContext &Context, const Camera &camera, uint32_t FrameIndex)
{
    DepthBuffer &Depth = gSceneDepthBuffer;
    ColorBuffer &LinearDepth = gLinearDepth[FrameIndex];
    const float NearClipDist = camera.GetNearClip();
    const float FarClipDist = camera.GetFarClip();
    const float zMagic = (FarClipDist - NearClipDist) / NearClipDist;

    LinearizeZ(Context, Depth, LinearDepth, zMagic);
}

bool SSAO::EnableDebugDraw()
{
    return sEnableDebugView;
}

void SSAO::ComputeAO(ComputeContext &Context, ColorBuffer &Destination, ColorBuffer &DepthBuffer,
                     const float TanHalfFovH)
{
    size_t BufferWidth = DepthBuffer.GetWidth();
    size_t BufferHeight = DepthBuffer.GetHeight();
    size_t ArrayCount = DepthBuffer.GetDepth();

    // ここでは、中心の深度値を各サンプル位置における球体の厚さ（の逆数）に変換する乗数を計算します。最大サンプル半径は5
    // 単位と想定していますが、球体にはその範囲に厚さがないため、そこまでサンプリングする必要はありません。
    // 距離が25未満の整数オフセットのサンプルのみが使用されます。つまり、
    // (3, 4) の距離はちょうど25（厚さは0）であるため、サンプルは存在しません。

    // シェーダーは、半径5ピクセル以内の円形領域をサンプリングするように設定されています。
    const float ScreenspaceDiameter = 10.0f;

    // SphereDiameter = CenterDepth *
    // ThicknessMultiplier。これは、特定の深度を中心とする球体の厚さを計算します。楕円体スケールによって球体が楕円体に引き伸ばされ、AOの特性が変化します。
    // TanHalfFovH: 球体の中心がZ = 1にある場合の深度単位での球体の半径
    // ScreenspaceDiameter: サンプル球体のピクセル単位での直径
    // ScreenspaceDiameter / BufferWidth: 球体が実際に覆う画面幅の比率
    // "2.0f * "に関する注意: Diameter = 2 * Radius
    float ThicknessMultiplier = 2.0f * TanHalfFovH * ScreenspaceDiameter / BufferWidth;

    if (ArrayCount == 1)
        ThicknessMultiplier *= 2.0f;

    // これにより、深度値が [0, 厚さ] から [0, 1] に変換されます。
    float InverseRangeFactor = 1.0f / ThicknessMultiplier;

    __declspec(align(16)) float SsaoCB[28];

    // The thicknesses are smaller for all off-center samples of the sphere.  Compute thicknesses relative
    // to the center sample.
    SsaoCB[0] = InverseRangeFactor / SampleThickness[0];
    SsaoCB[1] = InverseRangeFactor / SampleThickness[1];
    SsaoCB[2] = InverseRangeFactor / SampleThickness[2];
    SsaoCB[3] = InverseRangeFactor / SampleThickness[3];
    SsaoCB[4] = InverseRangeFactor / SampleThickness[4];
    SsaoCB[5] = InverseRangeFactor / SampleThickness[5];
    SsaoCB[6] = InverseRangeFactor / SampleThickness[6];
    SsaoCB[7] = InverseRangeFactor / SampleThickness[7];
    SsaoCB[8] = InverseRangeFactor / SampleThickness[8];
    SsaoCB[9] = InverseRangeFactor / SampleThickness[9];
    SsaoCB[10] = InverseRangeFactor / SampleThickness[10];
    SsaoCB[11] = InverseRangeFactor / SampleThickness[11];

    // これらはサンプルに掛けられる重みです。すべてのサンプルが同じように重要というわけではないからです。
    // サンプルが中心位置から遠いほど、重要度は低くなります。
    // 重みは球体の厚さで決定します。先頭のスカラーは、この重みを持つサンプルの数です。
    // 重みを掛ける前にサンプルを合計するため、
    // 全体としてはこれらのサンプルすべてがより重要になります。このテーブルを生成した後、重みは正規化されます。
    SsaoCB[12] = 4.0f * SampleThickness[0];  // Axial
    SsaoCB[13] = 4.0f * SampleThickness[1];  // Axial
    SsaoCB[14] = 4.0f * SampleThickness[2];  // Axial
    SsaoCB[15] = 4.0f * SampleThickness[3];  // Axial
    SsaoCB[16] = 4.0f * SampleThickness[4];  // Diagonal
    SsaoCB[17] = 8.0f * SampleThickness[5];  // L-shaped
    SsaoCB[18] = 8.0f * SampleThickness[6];  // L-shaped
    SsaoCB[19] = 8.0f * SampleThickness[7];  // L-shaped
    SsaoCB[20] = 4.0f * SampleThickness[8];  // Diagonal
    SsaoCB[21] = 8.0f * SampleThickness[9];  // L-shaped
    SsaoCB[22] = 8.0f * SampleThickness[10]; // L-shaped
    SsaoCB[23] = 4.0f * SampleThickness[11]; // Diagonal

    // #define SAMPLE_EXHAUSTIVELY

    // すべてのサンプルを使用していない場合は、正規化する前にそれらの重みを削除します。
#ifndef SAMPLE_EXHAUSTIVELY
    SsaoCB[12] = 0.0f;
    SsaoCB[14] = 0.0f;
    SsaoCB[17] = 0.0f;
    SsaoCB[19] = 0.0f;
    SsaoCB[21] = 0.0f;
#endif

    // すべての重みの合計で割って重みを正規化する
    float totalWeight = 0.0f;
    for (int i = 12; i < 24; ++i)
        totalWeight += SsaoCB[i];
    for (int i = 12; i < 24; ++i)
        SsaoCB[i] /= totalWeight;

    SsaoCB[24] = 1.0f / BufferWidth;
    SsaoCB[25] = 1.0f / BufferHeight;
    SsaoCB[26] = 1.0f / -sRejectionFalloff;
    SsaoCB[27] = 1.0f / (1.0f + sAccentuation);

    Context.SetDynamicConstantBufferView(1, sizeof(SsaoCB), SsaoCB);
    Context.SetDynamicDescriptor(2, 0, Destination.GetUAV());
    Context.SetDynamicDescriptor(3, 0, DepthBuffer.GetSRV());

    if (ArrayCount == 1)
        Context.Dispatch2D(BufferWidth, BufferHeight, 16, 16);
    else
        Context.Dispatch3D(BufferWidth, BufferHeight, ArrayCount, 8, 8, 1);
}

void SSAO::BlurAndUpsample(ComputeContext &Context, ColorBuffer &Destination, ColorBuffer &HiResDepth,
                           ColorBuffer &LoResDepth, ColorBuffer *InterleavedAO, ColorBuffer *HighQualityAO,
                           ColorBuffer *HiResAO)
{
    size_t LoWidth = LoResDepth.GetWidth();
    size_t LoHeight = LoResDepth.GetHeight();
    size_t HiWidth = HiResDepth.GetWidth();
    size_t HiHeight = HiResDepth.GetHeight();

    ComputePSO *shader = nullptr;
    if (HiResAO == nullptr)
    {
        shader = &sBlurUpsampleFinal[HighQualityAO == nullptr ? 0 : 1];
    }
    else
    {
        shader = &sBlurUpsampleBlend[HighQualityAO == nullptr ? 0 : 1];
    }
    Context.SetPipelineState(*shader);

    float kBlurTolerance = 1.0f - powf(10.0f, sBlurTolerance) * 1920.0f / (float)LoWidth;
    kBlurTolerance *= kBlurTolerance;
    float kUpsampleTolerance = powf(10.0f, sUpsampleTolerance);
    float kNoiseFilterWeight = 1.0f / (powf(10.0f, sNoiseFilterTolerance) + kUpsampleTolerance);

    __declspec(align(16)) float cbData[] = {1.0f / LoWidth,  1.0f / LoHeight,    1.0f / HiWidth,
                                            1.0f / HiHeight, kNoiseFilterWeight, 1920.0f / (float)LoWidth,
                                            kBlurTolerance,  kUpsampleTolerance};
    Context.SetDynamicConstantBufferView(1, sizeof(cbData), cbData);

    Context.TransitionResource(Destination, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(LoResDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(HiResDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.SetDynamicDescriptor(2, 0, Destination.GetUAV());
    Context.SetDynamicDescriptor(3, 0, LoResDepth.GetSRV());
    Context.SetDynamicDescriptor(3, 1, HiResDepth.GetSRV());
    if (InterleavedAO != nullptr)
    {
        Context.TransitionResource(*InterleavedAO, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.SetDynamicDescriptor(3, 2, InterleavedAO->GetSRV());
    }
    if (HighQualityAO != nullptr)
    {
        Context.TransitionResource(*HighQualityAO, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.SetDynamicDescriptor(3, 3, HighQualityAO->GetSRV());
    }
    if (HiResAO != nullptr)
    {
        Context.TransitionResource(*HiResAO, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Context.SetDynamicDescriptor(3, 4, HiResAO->GetSRV());
    }

    Context.Dispatch2D(HiWidth + 2, HiHeight + 2, 16, 16);
}
void SSAO::LinearizeZ(ComputeContext &Context, DepthBuffer &Depth, ColorBuffer &LinearDepth, float zMagic)
{
    Context.SetRootSignature(sRootSignature);
    Context.TransitionResource(Depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.SetConstants(0, zMagic);
    Context.SetDynamicDescriptor(3, 0, Depth.GetDepthSRV());
    Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.SetDynamicDescriptors(2, 0, 1, &LinearDepth.GetUAV());
    Context.SetPipelineState(sLinearizeDepthCS);
    Context.Dispatch2D(LinearDepth.GetWidth(), LinearDepth.GetHeight(), 16, 16);
}
} // namespace AtomEngine
