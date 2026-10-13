#include "LightManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Context/ComputeContext.h"
#include "Runtime/Core/Math/Random.h"

namespace AtomEngine
{
std::vector<LightData> LightManager::gLights;

StructuredBuffer LightManager::gLightBuffer;

std::unordered_map<uint32_t, uint32_t> LightManager::gLightIDToIndex;
uint32_t LightManager::gNextLightID = 0;

enum
{
    kMinLightGridDim = 8
};
enum
{
    shadowDim = 512
};
uint32_t LightManager::LightGridDim = 16;

Matrix4x4 LightManager::gLightShadowMatrix[MAX_LIGHTS];

RootSignature gFillLightRootSig;
ComputePSO gFillLightGridCS_8(L"Fill Light Grid 8 CS");
ComputePSO gFillLightGridCS_16(L"Fill Light Grid 16 CS");
ComputePSO gFillLightGridCS_24(L"Fill Light Grid 24 CS");
ComputePSO gFillLightGridCS_32(L"Fill Light Grid 32 CS");

bool LightManager::dirty = true;

std::vector<Vector3> LightManager::gLightVelocities;
std::vector<Vector3> LightManager::gLightAngularVel;
Vector3 LightManager::gMotionMinBound = Vector3::ZERO;
Vector3 LightManager::gMotionMaxBound = Vector3::ZERO;
bool LightManager::gMotionEnabled = false;

void LightManager::Initialize()
{
    gFillLightRootSig.Reset(3, 0);
    gFillLightRootSig[0].InitAsConstantBuffer(0);
    gFillLightRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
    gFillLightRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 2);
    gFillLightRootSig.Finalize(L"FillLightRS");

    auto fillLightCS_8 = ShaderCompiler::CompileBlob(L"Light/FillLightGridCS_8.hlsl", L"cs_6_2", L"CSMain");
    auto fillLightCS_16 = ShaderCompiler::CompileBlob(L"Light/FillLightGridCS_16.hlsl", L"cs_6_2", L"CSMain");
    auto fillLightCS_24 = ShaderCompiler::CompileBlob(L"Light/FillLightGridCS_24.hlsl", L"cs_6_2", L"CSMain");
    auto fillLightCS_32 = ShaderCompiler::CompileBlob(L"Light/FillLightGridCS_32.hlsl", L"cs_6_2", L"CSMain");

    gFillLightGridCS_8.SetRootSignature(gFillLightRootSig);
    gFillLightGridCS_8.SetComputeShader(fillLightCS_8.Get());
    gFillLightGridCS_8.Finalize();

    gFillLightGridCS_16.SetRootSignature(gFillLightRootSig);
    gFillLightGridCS_16.SetComputeShader(fillLightCS_16.Get());
    gFillLightGridCS_16.Finalize();

    gFillLightGridCS_24.SetRootSignature(gFillLightRootSig);
    gFillLightGridCS_24.SetComputeShader(fillLightCS_24.Get());
    gFillLightGridCS_24.Finalize();

    gFillLightGridCS_32.SetRootSignature(gFillLightRootSig);
    gFillLightGridCS_32.SetComputeShader(fillLightCS_32.Get());
    gFillLightGridCS_32.Finalize();

    // Assumes max resolution of 3840x2160
    uint32_t lightGridCells = DivideByMultiple(3840, kMinLightGridDim) * DivideByMultiple(2160, kMinLightGridDim);
    uint32_t lightGridSizeBytes = lightGridCells * (4 + MAX_LIGHTS * 4);

    uint32_t lightGridBitMaskSizeBytes = lightGridCells * 4 * 4;

    gLightBuffer.Create(L"LightBuffer", MAX_LIGHTS, sizeof(LightData));
    gLights.reserve(MAX_LIGHTS);
}

uint32_t LightManager::AddLight(const LightDesc &desc)
{
    LightData lightData;
    lightData.position = desc.position;
    lightData.radiusSq = desc.radius * desc.radius;
    lightData.color = desc.color.ToVector3();
    lightData.type = static_cast<uint32_t>(desc.type);
    lightData.direction = desc.direction.NormalizedCopy();
    lightData.innerCos = Math::cos(desc.innerAngle);
    lightData.outerCos = Math::cos(desc.outerAngle);
    lightData.shadowMatrix = desc.shadowMatrix;
    lightData.isActive = true;
    if (gLights.size() >= MAX_LIGHTS)
    {
        return 0;
    }
    gLights.push_back(lightData);

    uint32_t id = gNextLightID++;
    gLightIDToIndex[id] = (uint32_t)gLights.size() - 1;
    dirty = true;
    return id;
}

uint32_t LightManager::AddPointLight(const Vector3 &position, const Color &color, float radius)
{
    LightData lightData;
    lightData.color = color.ToVector3();
    lightData.position = position;
    lightData.radiusSq = radius * radius;
    lightData.type = (uint32_t)LightType::Point;
    lightData.isActive = true;
    if (gLights.size() >= MAX_LIGHTS)
    {
        return 0;
    }
    gLights.push_back(lightData);
    uint32_t id = gNextLightID++;
    gLightIDToIndex[id] = (uint32_t)gLights.size() - 1;
    dirty = true;
    return id;
}

void LightManager::RemoveLight(uint32_t lightID)
{
    auto it = gLightIDToIndex.find(lightID);
    if (it == gLightIDToIndex.end())
        return;

    uint32_t removedIndex = it->second;
    uint32_t lastIndex = (uint32_t)gLights.size() - 1;

    gLights[removedIndex] = gLights[lastIndex];
    gLights.pop_back();

    for (auto &pair : gLightIDToIndex)
    {
        if (pair.second == lastIndex)
        {
            pair.second = removedIndex;
            break;
        }
    }
    gLightIDToIndex.erase(it);
    dirty = true;
}

void LightManager::UploadResources()
{
    if (gLights.empty())
        return;
    if (dirty)
    {
        for (uint32_t i = 0; i < gLights.size(); ++i)
        {
            const Vector3 &pos = gLights[i].position;
            const Vector3 &dir = gLights[i].direction;
            float radius = Math::Sqrt(gLights[i].radiusSq);
            float outerCos = gLights[i].outerCos;

            Camera shadowCamera;
            shadowCamera.SetLookAt(pos, pos + dir, Vector3::UP);
            shadowCamera.SetPerspectiveMatrix(outerCos * 2, 1.0f, radius * .05f, radius * 1.0f);
            shadowCamera.Update();

            gLightShadowMatrix[i] = shadowCamera.GetViewProjMatrix();
            Matrix4x4 shadowTextureMatrix =
                gLightShadowMatrix[i] * Matrix4x4(Matrix3x3::MakeScale({0.5f, -0.5f, 1.0f}), Vector3(0.5f, 0.5f, 0.0f));
            gLights[i].shadowMatrix = shadowTextureMatrix;
        }

        CommandContext::InitializeBuffer(gLightBuffer, gLights.data(), gLights.size() * (sizeof(LightData)));
        dirty = false;
    }
}

void LightManager::UpdateLight(uint32_t lightID, const LightDesc &desc)
{
    auto it = gLightIDToIndex.find(lightID);
    if (it == gLightIDToIndex.end())
        return;

    uint32_t idx = it->second;
    LightData &data = gLights[idx];

    data.position = desc.position;
    data.radiusSq = desc.radius * desc.radius;
    data.color = desc.color.ToVector3();
    data.type = static_cast<uint32_t>(desc.type);
    data.direction = desc.direction.NormalizedCopy();
    data.innerCos = std::cos(desc.innerAngle);
    data.outerCos = std::cos(desc.outerAngle);
    dirty = true;
}

void LightManager::UpdatePointLight(uint32_t lightID, const Vector3 &position, const Color &color, float radius)
{
    auto it = gLightIDToIndex.find(lightID);
    if (it == gLightIDToIndex.end())
        return;

    LightData &data = gLights[it->second];
    data.position = position;
    data.radiusSq = radius * radius;
    data.color = color.ToVector3();
    dirty = true;
}

void LightManager::CreateRandomLights(const Vector3 minBound, const Vector3 maxBound)
{
    Vector3 posScale = maxBound - minBound;
    Vector3 posBias = minBound;

    // todo: replace this with MT
    srand((uint32_t)time(0));
    auto randUint = []() -> uint32_t {
        return rand(); // [0, RAND_MAX]
    };
    auto randFloat = [randUint]() -> float {
        return randUint() * (1.0f / RAND_MAX); // convert [0, RAND_MAX] to [0, 1]
    };
    auto randVecUniform = [randFloat]() -> Vector3 { return Vector3(randFloat(), randFloat(), randFloat()); };
    auto randGaussian = [randFloat]() -> float {
        // polar box-muller
        static bool gaussianPair = true;
        static float y2;

        if (gaussianPair)
        {
            gaussianPair = false;

            float x1, x2, w;
            do
            {
                x1 = 2 * randFloat() - 1;
                x2 = 2 * randFloat() - 1;
                w = x1 * x1 + x2 * x2;
            } while (w >= 1);

            w = sqrtf(-2 * logf(w) / w);
            y2 = x2 * w;
            return x1 * w;
        }
        else
        {
            gaussianPair = true;
            return y2;
        }
    };
    auto randVecGaussian = [randGaussian]() -> Vector3 {
        return Math::Normalize(Vector3(randGaussian(), randGaussian(), randGaussian()));
    };

    gLights.resize(MAX_LIGHTS);

    // Clear existing ID mappings
    gLightIDToIndex.clear();

    // Prepare motion arrays
    gLightVelocities.clear();
    gLightAngularVel.clear();
    gLightVelocities.resize(MAX_LIGHTS);
    gLightAngularVel.resize(MAX_LIGHTS);

    // store bounds
    gMotionMinBound = minBound;
    gMotionMaxBound = maxBound;

    for (uint32_t n = 0; n < MAX_LIGHTS; n++)
    {
        Vector3 pos = randVecUniform() * posScale + posBias;
        float lightRadius = randFloat() * 800.0f + 200.0f;

        Vector3 color = randVecUniform();
        float colorScale = randFloat() * .3f + .3f;
        color = color * colorScale;

        uint32_t type;
        // force types to match 32-bit boundaries for the BIT_MASK_SORTED case
        if (n < 32 * 1)
            type = 0;
        else if (n < 32 * 3)
            type = 1;
        else
            type = 2;

        Vector3 coneDir = randVecGaussian();
        float coneInner = (randFloat() * .2f + .025f) * Math::PI;
        float coneOuter = coneInner + randFloat() * .1f * Math::PI;

        if (type == 1 || type == 2)
        {
            // emphasize cone lights
            color = color * 5.0f;
        }

        Camera shadowCamera;
        shadowCamera.SetLookAt(pos, pos + coneDir, Vector3(0, 1, 0));
        shadowCamera.SetPerspectiveMatrix(coneOuter * 2, 1.0f, lightRadius * .05f, lightRadius * 1.0f);
        shadowCamera.Update();
        gLightShadowMatrix[n] = shadowCamera.GetViewProjMatrix();
        Matrix4x4 shadowTextureMatrix =
            gLightShadowMatrix[n] * Matrix4x4(Matrix3x3::MakeScale({0.5f, -0.5f, 1.0f}), Vector3({0.5f, 0.5f, 0.0f}));

        gLights[n].position = pos;
        gLights[n].radiusSq = lightRadius * lightRadius;
        gLights[n].color = color;
        gLights[n].type = type;
        gLights[n].direction = coneDir;
        gLights[n].innerCos = cosf(coneInner);
        gLights[n].outerCos = cosf(coneOuter);
        gLights[n].isActive = true;
        std::memcpy(&gLights[n].shadowMatrix, &shadowTextureMatrix, sizeof(shadowTextureMatrix));

        // Update ID mapping
        uint32_t lightID = gNextLightID++;
        gLightIDToIndex[lightID] = n;

        // 初期速度（単位: world units / sec）・角速度（軸 * rad/sec）
        float speed = randFloat() * 400.0f + 50.0f; // 0.5 〜 4.5
        gLightVelocities[n] = randVecGaussian() * speed;

        float angSpeed = randFloat() * 1.2f; // 0 〜 ~1.2 rad/s
        gLightAngularVel[n] = randVecGaussian() * angSpeed;
    }

    CommandContext::InitializeBuffer(gLightBuffer, gLights.data(), MAX_LIGHTS * sizeof(LightData));
    dirty = false;
}

void LightManager::FillLightGrid(GraphicsContext &gfxContext, const Camera &camera)
{
    ComputeContext &Context = gfxContext.GetComputeContext();

    Context.SetRootSignature(gFillLightRootSig);

    switch (LightGridDim)
    {
    case 8:
        Context.SetPipelineState(gFillLightGridCS_8);
        break;
    case 16:
        Context.SetPipelineState(gFillLightGridCS_16);
        break;
    case 24:
        Context.SetPipelineState(gFillLightGridCS_24);
        break;
    case 32:
        Context.SetPipelineState(gFillLightGridCS_32);
        break;
    default:
        ASSERT(false);
        break;
    }

    // ColorBuffer& LinearDepth = gLinearDepth[DX12Core::GetFrameIndex()];

    Context.TransitionResource(gLightBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    // Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gViewLightGrid, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(gViewLightGridBitMask, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    Context.SetDynamicDescriptor(1, 0, gLightBuffer.GetSRV());
    // Context.SetDynamicDescriptor(1, 1, LinearDepth.GetSRV());
    Context.SetDynamicDescriptor(1, 1, gSceneDepthBuffer.GetDepthSRV());
    Context.SetDynamicDescriptor(2, 0, gViewLightGrid.GetUAV());
    Context.SetDynamicDescriptor(2, 1, gViewLightGridBitMask.GetUAV());

    uint32_t tileCountX = DivideByMultiple(gSceneColorBuffer.GetWidth(), LightGridDim);
    uint32_t tileCountY = DivideByMultiple(gSceneColorBuffer.GetHeight(), LightGridDim);

    float FarClipDist = camera.GetFarClip();
    float NearClipDist = camera.GetNearClip();
    const float RcpZMagic = NearClipDist / (FarClipDist - NearClipDist);

    struct CSConstants
    {
        Matrix4x4 ViewMatrix;
        Matrix4x4 ProjMatrix;
        Matrix4x4 ProjInverse;
        uint32_t ViewportWidth, ViewportHeight;
        float InvTileDim;
        float RcpZMagic;
        uint32_t TileCount;
    } csConstants;
    // todo: assumes 1920x1080 resolution
    csConstants.ViewportWidth = gSceneColorBuffer.GetWidth();
    csConstants.ViewportHeight = gSceneColorBuffer.GetHeight();
    csConstants.InvTileDim = 1.0f / LightGridDim;
    csConstants.RcpZMagic = RcpZMagic;
    csConstants.TileCount = tileCountX;
    csConstants.ViewMatrix = camera.GetViewMatrix();
    const auto &proj = camera.GetProjMatrix();
    csConstants.ProjMatrix = proj;
    csConstants.ProjInverse = proj.Inverse();
    Context.SetDynamicConstantBufferView(0, sizeof(CSConstants), &csConstants);

    Context.Dispatch(tileCountX, tileCountY, 1);

    Context.TransitionResource(gLightBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gViewLightGrid, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(gViewLightGridBitMask, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void LightManager::Shutdown()
{
    gLightIDToIndex.clear();

    gLightBuffer.Destroy();
}
void LightManager::EnableRandomMotion(bool enable)
{
    gMotionEnabled = enable;
}

void LightManager::UpdateRandomMotion(float deltaTime)
{
    if (!gMotionEnabled)
        return;
    if (gLights.empty())
        return;

    for (size_t i = 0; i < gLights.size(); ++i)
    {
        // ポイントライトは位置を移動してバウンド内で反射
        if (gLights[i].type == static_cast<uint32_t>(LightType::Point))
        {
            Vector3 pos = gLights[i].position;
            Vector3 vel = (i < gLightVelocities.size()) ? gLightVelocities[i] : Vector3::ZERO;

            pos += vel * deltaTime;

            // バウンドチェックと反射
            if (pos.x < gMotionMinBound.x)
            {
                pos.x = gMotionMinBound.x;
                vel.x = -vel.x;
            }
            else if (pos.x > gMotionMaxBound.x)
            {
                pos.x = gMotionMaxBound.x;
                vel.x = -vel.x;
            }
            if (pos.y < gMotionMinBound.y)
            {
                pos.y = gMotionMinBound.y;
                vel.y = -vel.y;
            }
            else if (pos.y > gMotionMaxBound.y)
            {
                pos.y = gMotionMaxBound.y;
                vel.y = -vel.y;
            }
            if (pos.z < gMotionMinBound.z)
            {
                pos.z = gMotionMinBound.z;
                vel.z = -vel.z;
            }
            else if (pos.z > gMotionMaxBound.z)
            {
                pos.z = gMotionMaxBound.z;
                vel.z = -vel.z;
            }

            gLights[i].position = pos;
            if (i < gLightVelocities.size())
                gLightVelocities[i] = vel;
            dirty = true;
        }
        else
        {
            // スポット系: 角速度ベクトルを用いて方向を回転
            if (i >= gLightAngularVel.size())
                continue;
            Vector3 ang = gLightAngularVel[i];
            float angMag = ang.Length();
            if (angMag > 1e-6f)
            {
                Vector3 axis = ang / angMag;
                float angle = angMag * deltaTime; // ラジアン
                Quaternion q(axis, angle);
                Vector3 newDir = q * gLights[i].direction;
                newDir.Normalize();
                gLights[i].direction = newDir;
                dirty = true;
            }
        }
    }
}
} // namespace AtomEngine
