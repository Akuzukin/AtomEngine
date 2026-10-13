#pragma once
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "ShadowBuffer.h"
#include "GpuBuffer.h"
#include "UploadBuffer.h"
#include "Runtime/Core/Math/Matrix4x4.h"
namespace AtomEngine
{
// All writable screen-space resources belong to one view. Pipelines remain shared.
struct ViewBuffers
{
    DepthBuffer sceneDepthBuffer;
    ColorBuffer sceneColorBuffer;
    ColorBuffer sceneNormalBuffer;
    ColorBuffer postEffectsBuffer;
    ColorBuffer overlayBuffer;
    ColorBuffer velocityBuffer;
    ShadowBuffer shadowBuffer;
    ShadowBuffer rayTracedShadowBuffer;
    ColorBuffer sSAOFullScreen;
    ColorBuffer linearDepth[2];
    ColorBuffer depthDownsize1;
    ColorBuffer depthDownsize2;
    ColorBuffer depthDownsize3;
    ColorBuffer depthDownsize4;
    ColorBuffer depthTiled1;
    ColorBuffer depthTiled2;
    ColorBuffer depthTiled3;
    ColorBuffer depthTiled4;
    ColorBuffer aOMerged1;
    ColorBuffer aOMerged2;
    ColorBuffer aOMerged3;
    ColorBuffer aOMerged4;
    ColorBuffer aOSmooth1;
    ColorBuffer aOSmooth2;
    ColorBuffer aOSmooth3;
    ColorBuffer aOHighQuality1;
    ColorBuffer aOHighQuality2;
    ColorBuffer aOHighQuality3;
    ColorBuffer aOHighQuality4;
    ColorBuffer motionPrepBuffer;
    ColorBuffer lumaBuffer;
    ColorBuffer bloomUAV1[2];
    ColorBuffer bloomUAV2[2];
    ColorBuffer bloomUAV3[2];
    ColorBuffer bloomUAV4[2];
    ColorBuffer bloomUAV5[2];
    ColorBuffer lumaLR;
    ByteAddressBuffer histogram;
    ColorBuffer selectionMask;
    ColorBuffer fxaaBuffer;
    StructuredBuffer viewExposure;
    ByteAddressBuffer viewLightGrid;
    ByteAddressBuffer viewLightGridBitMask;
    ColorBuffer viewLightShadowArray;
    ShadowBuffer viewLightShadowTemp;
    uint64_t frameIndex = 0;
    Matrix4x4 previousViewProj, reprojection;
    bool hasHistory = false;
    void Create(uint32_t width, uint32_t height);
    void Destroy();
};
ViewBuffers &GetViewBuffers();
ViewBuffers &GetPrimaryViewBuffers();
class ScopedViewBuffers
{
    ViewBuffers *previous;

  public:
    explicit ScopedViewBuffers(ViewBuffers &buffers);
    ~ScopedViewBuffers();
    ScopedViewBuffers(const ScopedViewBuffers &) = delete;
    ScopedViewBuffers &operator=(const ScopedViewBuffers &) = delete;
};
void InitializeBuffers(uint32_t width, uint32_t height);
void DestroyBuffers();
void ResizeBuffers(uint32_t width, uint32_t height);
} // namespace AtomEngine
// Compatibility names route existing effects to the resources of the active view.
#define gSceneDepthBuffer GetViewBuffers().sceneDepthBuffer
#define gSceneColorBuffer GetViewBuffers().sceneColorBuffer
#define gSceneNormalBuffer GetViewBuffers().sceneNormalBuffer
#define gPostEffectsBuffer GetViewBuffers().postEffectsBuffer
#define gOverlayBuffer GetViewBuffers().overlayBuffer
#define gVelocityBuffer GetViewBuffers().velocityBuffer
#define gShadowBuffer GetViewBuffers().shadowBuffer
#define gRayTracedShadowBuffer GetViewBuffers().rayTracedShadowBuffer
#define gSSAOFullScreen GetViewBuffers().sSAOFullScreen
#define gLinearDepth GetViewBuffers().linearDepth
#define gDepthDownsize1 GetViewBuffers().depthDownsize1
#define gDepthDownsize2 GetViewBuffers().depthDownsize2
#define gDepthDownsize3 GetViewBuffers().depthDownsize3
#define gDepthDownsize4 GetViewBuffers().depthDownsize4
#define gDepthTiled1 GetViewBuffers().depthTiled1
#define gDepthTiled2 GetViewBuffers().depthTiled2
#define gDepthTiled3 GetViewBuffers().depthTiled3
#define gDepthTiled4 GetViewBuffers().depthTiled4
#define gAOMerged1 GetViewBuffers().aOMerged1
#define gAOMerged2 GetViewBuffers().aOMerged2
#define gAOMerged3 GetViewBuffers().aOMerged3
#define gAOMerged4 GetViewBuffers().aOMerged4
#define gAOSmooth1 GetViewBuffers().aOSmooth1
#define gAOSmooth2 GetViewBuffers().aOSmooth2
#define gAOSmooth3 GetViewBuffers().aOSmooth3
#define gAOHighQuality1 GetViewBuffers().aOHighQuality1
#define gAOHighQuality2 GetViewBuffers().aOHighQuality2
#define gAOHighQuality3 GetViewBuffers().aOHighQuality3
#define gAOHighQuality4 GetViewBuffers().aOHighQuality4
#define gMotionPrepBuffer GetViewBuffers().motionPrepBuffer
#define gLumaBuffer GetViewBuffers().lumaBuffer
#define gBloomUAV1 GetViewBuffers().bloomUAV1
#define gBloomUAV2 GetViewBuffers().bloomUAV2
#define gBloomUAV3 GetViewBuffers().bloomUAV3
#define gBloomUAV4 GetViewBuffers().bloomUAV4
#define gBloomUAV5 GetViewBuffers().bloomUAV5
#define gLumaLR GetViewBuffers().lumaLR
#define gHistogram GetViewBuffers().histogram
#define gSelectionMask GetViewBuffers().selectionMask
#define gFxaaBuffer GetViewBuffers().fxaaBuffer
#define gViewExposure GetViewBuffers().viewExposure
#define gViewLightGrid GetViewBuffers().viewLightGrid
#define gViewLightGridBitMask GetViewBuffers().viewLightGridBitMask
#define gViewLightShadowArray GetViewBuffers().viewLightShadowArray
#define gViewLightShadowTemp GetViewBuffers().viewLightShadowTemp
