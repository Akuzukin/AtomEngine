#pragma once
#include "Runtime/Platform/DirectX12/Shader/LightData.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Function/Camera/ShadowCamera.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"

namespace AtomEngine
{
struct LightDesc
{
    LightType type = LightType::Point;

    Vector3 position = {0, 0, 0};
    Vector3 direction = {0, -1, 0};
    Color color = {1, 1, 1, 1};
    float radius = 1.0f;
    Radian innerAngle;
    Radian outerAngle;
    Matrix4x4 shadowMatrix;
};

class LightManager
{
  public:
    static void Initialize();

    static uint32_t AddLight(const LightDesc &desc);

    static uint32_t AddPointLight(const Vector3 &position, const Color &color, float radius);
    static void RemoveLight(uint32_t lightID);

    static void UploadResources();
    static void UpdateLight(uint32_t lightID, const LightDesc &desc);
    static void UpdatePointLight(uint32_t lightID, const Vector3 &position, const Color &color, float radius);

    static void CreateRandomLights(const Vector3 minBound, const Vector3 maxBound);

    static void FillLightGrid(GraphicsContext &gfxContext, const Camera &camera);

    static void Shutdown();

    static void EnableRandomMotion(bool enable);
    static void UpdateRandomMotion(float deltaTime);

    static D3D12_CPU_DESCRIPTOR_HANDLE GetLightSrv()
    {
        return gLightBuffer.GetSRV();
    }
    static D3D12_CPU_DESCRIPTOR_HANDLE GetLightShadowSrv()
    {
        return gViewLightShadowArray.GetSRV();
    }
    static D3D12_CPU_DESCRIPTOR_HANDLE GetLightGridSrv()
    {
        return gViewLightGrid.GetSRV();
    }
    static D3D12_CPU_DESCRIPTOR_HANDLE GetLightGridBitMaskSrv()
    {
        return gViewLightGridBitMask.GetSRV();
    }

    static ColorBuffer &GetLightShadowArray()
    {
        return gViewLightShadowArray;
    }
    static ShadowBuffer &GetLightShadowTempBuffer()
    {
        return gViewLightShadowTemp;
    }

  public:
    static uint32_t LightGridDim;

  private:
    static std::vector<LightData> gLights;

    static StructuredBuffer gLightBuffer;

    static Matrix4x4 gLightShadowMatrix[MAX_LIGHTS];

    static std::unordered_map<uint32_t, uint32_t> gLightIDToIndex;
    static uint32_t gNextLightID;
    static bool dirty;

    // ランダムモーション用データ
    static std::vector<Vector3> gLightVelocities; // 移動速度（ワールド単位/秒）
    static std::vector<Vector3> gLightAngularVel; // 角速度ベクトル（軸 * rad/s）
    static Vector3 gMotionMinBound;
    static Vector3 gMotionMaxBound;
    static bool gMotionEnabled;
};
} // namespace AtomEngine
