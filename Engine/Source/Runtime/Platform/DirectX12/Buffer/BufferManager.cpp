#include "BufferManager.h"
#include "../Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"

namespace AtomEngine
{
namespace
{
ViewBuffers primary;
thread_local ViewBuffers *active = &primary;
} // namespace
ViewBuffers &GetViewBuffers()
{
    return *active;
}
ViewBuffers &GetPrimaryViewBuffers()
{
    return primary;
}
ScopedViewBuffers::ScopedViewBuffers(ViewBuffers &buffers) : previous(active)
{
    active = &buffers;
}
ScopedViewBuffers::~ScopedViewBuffers()
{
    active = previous;
}
void ViewBuffers::Create(uint32_t width, uint32_t height)
{
    frameIndex = 0;
    hasHistory = false;
    ScopedViewBuffers scope(*this);
    InitializeBuffers(width, height);
}
void ViewBuffers::Destroy()
{
    ScopedViewBuffers scope(*this);
    DestroyBuffers();
}
void InitializeBuffers(uint32_t bufferWidth, uint32_t bufferHeight)
{
    GetViewBuffers().frameIndex = 0;
    GetViewBuffers().hasHistory = false;

    const uint32_t bufferWidth1 = (bufferWidth + 1) / 2;
    const uint32_t bufferWidth2 = (bufferWidth + 3) / 4;
    const uint32_t bufferWidth3 = (bufferWidth + 7) / 8;
    const uint32_t bufferWidth4 = (bufferWidth + 15) / 16;
    const uint32_t bufferWidth5 = (bufferWidth + 31) / 32;
    const uint32_t bufferWidth6 = (bufferWidth + 63) / 64;
    const uint32_t bufferHeight1 = (bufferHeight + 1) / 2;
    const uint32_t bufferHeight2 = (bufferHeight + 3) / 4;
    const uint32_t bufferHeight3 = (bufferHeight + 7) / 8;
    const uint32_t bufferHeight4 = (bufferHeight + 15) / 16;
    const uint32_t bufferHeight5 = (bufferHeight + 31) / 32;
    const uint32_t bufferHeight6 = (bufferHeight + 63) / 64;

    gSSAOFullScreen.SetClearColor(Color(1, 1, 1));
    gSceneColorBuffer.SetClearColor(Color(0x314D79FF));
    gSceneColorBuffer.Create(L"Main Color Buffer", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
    gVelocityBuffer.Create(L"Motion Vectors", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R32_UINT);
    gSceneNormalBuffer.Create(L"Normals Buffer", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
    gPostEffectsBuffer.Create(L"Post Effects Buffer", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R32_UINT);

    gSelectionMask.Create(L"View Selection Mask", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R8_UNORM);
    gFxaaBuffer.Create(L"View FXAA", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
    alignas(16) float exposure[] = {1, 1, 1, 0, -12, 4, 16, 1.f / 16};
    gViewExposure.Create(L"View Exposure", 8, 4, exposure);
    // Light grids are sized for this view rather than for a fixed maximum display size.
    uint32_t cells = ((bufferWidth + 7) / 8) * ((bufferHeight + 7) / 8);
    gViewLightGrid.Create(L"View Light Grid", cells * (4 + 128 * 4), 1);
    gViewLightGridBitMask.Create(L"View Light Bit Mask", cells * 16, 1);
    gViewLightShadowArray.CreateArray(L"View Light Shadows", 512, 512, 128, DXGI_FORMAT_R16_UNORM);
    gViewLightShadowTemp.Create(L"View Light Shadow Temp", 512, 512);
    gSceneDepthBuffer.Create(L"Scene Depth Buffer", bufferWidth, bufferHeight, DXGI_FORMAT_D24_UNORM_S8_UINT);

    // The primary composite reads Overlay by swap-chain pixel coordinates.
    // Rebuilding the 3D resolution must not resize this buffer to the scene dimensions.
    // Offscreen views still own an overlay matching their own render target.
    bool primaryView=std::addressof(GetViewBuffers())==std::addressof(GetPrimaryViewBuffers());
    auto overlayWidth=primaryView && DX12Core::gBackBufferWidth ? DX12Core::gBackBufferWidth : bufferWidth;
    auto overlayHeight=primaryView && DX12Core::gBackBufferHeight ? DX12Core::gBackBufferHeight : bufferHeight;
    gOverlayBuffer.Create(L"UI Overlay", overlayWidth, overlayHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

    gShadowBuffer.Create(L"Shadow Map", 2048, 2048);
    gRayTracedShadowBuffer.Create(L"Ray Traced Shadow Map", bufferWidth, bufferHeight);

    // SSAO
    {
        gSSAOFullScreen.Create(L"SSAO Full Screen", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R8_UNORM);
        gLinearDepth[0].Create(L"Linear Depth 0", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16_UNORM);
        gLinearDepth[1].Create(L"Linear Depth 1", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16_UNORM);
        gDepthDownsize1.Create(L"Depth Down-Sized 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R32_FLOAT);
        gDepthDownsize2.Create(L"Depth Down-Sized 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R32_FLOAT);
        gDepthDownsize3.Create(L"Depth Down-Sized 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R32_FLOAT);
        gDepthDownsize4.Create(L"Depth Down-Sized 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R32_FLOAT);
        gDepthTiled1.CreateArray(L"Depth De-Interleaved 1", bufferWidth3, bufferHeight3, 16, DXGI_FORMAT_R16_FLOAT);
        gDepthTiled2.CreateArray(L"Depth De-Interleaved 2", bufferWidth4, bufferHeight4, 16, DXGI_FORMAT_R16_FLOAT);
        gDepthTiled3.CreateArray(L"Depth De-Interleaved 3", bufferWidth5, bufferHeight5, 16, DXGI_FORMAT_R16_FLOAT);
        gDepthTiled4.CreateArray(L"Depth De-Interleaved 4", bufferWidth6, bufferHeight6, 16, DXGI_FORMAT_R16_FLOAT);
        gAOMerged1.Create(L"AO Re-Interleaved 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM);
        gAOMerged2.Create(L"AO Re-Interleaved 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM);
        gAOMerged3.Create(L"AO Re-Interleaved 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM);
        gAOMerged4.Create(L"AO Re-Interleaved 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM);
        gAOSmooth1.Create(L"AO Smoothed 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM);
        gAOSmooth2.Create(L"AO Smoothed 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM);
        gAOSmooth3.Create(L"AO Smoothed 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM);
        gAOHighQuality1.Create(L"AO High Quality 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM);
        gAOHighQuality2.Create(L"AO High Quality 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM);
        gAOHighQuality3.Create(L"AO High Quality 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM);
        gAOHighQuality4.Create(L"AO High Quality 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM);
    }

    // Motion Blur
    {
        gMotionPrepBuffer.Create(L"Motion Blur Prep", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
    }
    // Luminance
    {
        gLumaBuffer.Create(L"Luminance", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R8_UNORM);
        uint32_t kBloomWidth = bufferWidth > 2560 ? 1280 : 640;
        uint32_t kBloomHeight = bufferHeight > 1440 ? 768 : 384;
        gLumaLR.Create(L"Luma Buffer", kBloomWidth, kBloomHeight, 1, DXGI_FORMAT_R8_UINT);
        gBloomUAV1[0].Create(L"Bloom Buffer 1a", kBloomWidth, kBloomHeight, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV1[1].Create(L"Bloom Buffer 1b", kBloomWidth, kBloomHeight, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV2[0].Create(L"Bloom Buffer 2a", kBloomWidth / 2, kBloomHeight / 2, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV2[1].Create(L"Bloom Buffer 2b", kBloomWidth / 2, kBloomHeight / 2, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV3[0].Create(L"Bloom Buffer 3a", kBloomWidth / 4, kBloomHeight / 4, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV3[1].Create(L"Bloom Buffer 3b", kBloomWidth / 4, kBloomHeight / 4, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV4[0].Create(L"Bloom Buffer 4a", kBloomWidth / 8, kBloomHeight / 8, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV4[1].Create(L"Bloom Buffer 4b", kBloomWidth / 8, kBloomHeight / 8, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV5[0].Create(L"Bloom Buffer 5a", kBloomWidth / 16, kBloomHeight / 16, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
        gBloomUAV5[1].Create(L"Bloom Buffer 5b", kBloomWidth / 16, kBloomHeight / 16, 1, DXGI_FORMAT_R10G10B10A2_UNORM);

        gHistogram.Create(L"Histogram", 256, 4);
    }
}
void DestroyBuffers()
{
    gSelectionMask.Destroy();
    gFxaaBuffer.Destroy();
    gViewExposure.Destroy();
    gViewLightGrid.Destroy();
    gViewLightGridBitMask.Destroy();
    gViewLightShadowArray.Destroy();
    gViewLightShadowTemp.Destroy();
    gSceneColorBuffer.Destroy();
    gSceneNormalBuffer.Destroy();
    gPostEffectsBuffer.Destroy();
    gOverlayBuffer.Destroy();
    gVelocityBuffer.Destroy();

    gSceneDepthBuffer.Destroy();

    gShadowBuffer.Destroy();
    gRayTracedShadowBuffer.Destroy();

    gSSAOFullScreen.Destroy();
    gLinearDepth[0].Destroy();
    gLinearDepth[1].Destroy();
    gDepthDownsize1.Destroy();
    gDepthDownsize2.Destroy();
    gDepthDownsize3.Destroy();
    gDepthDownsize4.Destroy();
    gDepthTiled1.Destroy();
    gDepthTiled2.Destroy();
    gDepthTiled3.Destroy();
    gDepthTiled4.Destroy();
    gAOMerged1.Destroy();
    gAOMerged2.Destroy();
    gAOMerged3.Destroy();
    gAOMerged4.Destroy();
    gAOSmooth1.Destroy();
    gAOSmooth2.Destroy();
    gAOSmooth3.Destroy();
    gAOHighQuality1.Destroy();
    gAOHighQuality2.Destroy();
    gAOHighQuality3.Destroy();
    gAOHighQuality4.Destroy();

    gLumaBuffer.Destroy();
    gLumaLR.Destroy();

    gMotionPrepBuffer.Destroy();
    gBloomUAV1[0].Destroy();
    gBloomUAV1[1].Destroy();
    gBloomUAV2[0].Destroy();
    gBloomUAV2[1].Destroy();
    gBloomUAV3[0].Destroy();
    gBloomUAV3[1].Destroy();
    gBloomUAV4[0].Destroy();
    gBloomUAV4[1].Destroy();
    gBloomUAV5[0].Destroy();
    gBloomUAV5[1].Destroy();
    gHistogram.Destroy();
}
void ResizeBuffers(uint32_t Width, uint32_t Height)
{
    if (DX12Core::gBackBufferWidth != 0 && DX12Core::gBackBufferHeight != 0)
        GetPrimaryViewBuffers().overlayBuffer.Create(L"UI Overlay", DX12Core::gBackBufferWidth, DX12Core::gBackBufferHeight, 1,
                              DXGI_FORMAT_R8G8B8A8_UNORM);
}
} // namespace AtomEngine
