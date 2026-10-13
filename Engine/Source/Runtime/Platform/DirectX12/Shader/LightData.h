#ifndef LIGHT_DATA_H
#define LIGHT_DATA_H
#endif

#ifdef __cplusplus
#include "Runtime/Core/Math/MathInclude.h"
using float2 = AtomEngine::Vector2;
using float3 = AtomEngine::Vector3;
using float4x4 = AtomEngine::Matrix4x4;
using uint = uint32_t;

enum class LightType : uint
{
    Point = 0,      // Point light
    Spot,           // Spot light
    SpotShadow,     // Spot light with shadow
};
#endif // 

#define MAX_LIGHTS 128
#define TILE_SIZE (4 + MAX_LIGHTS * 4)

struct LightData
{
    float3 position;
    float radiusSq;
    float3 color;

    uint type;
    float3 direction;
    float innerCos;
    float outerCos;
    uint isActive;
    float2 pad;

    float4x4 shadowMatrix;
};
