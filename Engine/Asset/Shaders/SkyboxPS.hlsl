#include "Common.hlsli"
#include "ProceduralSky.hlsli"

cbuffer PSConstants : register(b0)
{
    SkyParameters Sky;
};

TextureCube<float3> skyCube : register(t0);
Texture2D<float3> skyPanorama : register(t1);

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 viewDir : TEXCOORD3;
};

float4 main(VSOutput input) : SV_Target0
{
    float3 direction = normalize(input.viewDir);
    float3 color;
    if (Sky.GroundAndMode.w > 0.5)
    {
        float sine, cosine;
        sincos(Sky.HorizonAndRotation.w, sine, cosine);
        direction.xz = float2(direction.x * cosine - direction.z * sine,
                              direction.x * sine + direction.z * cosine);
        if (Sky.GroundAndMode.w < 1.5)
            color = skyCube.SampleLevel(defaultSampler, direction, 0);
        else
        {
            float2 uv = float2(atan2(direction.z, direction.x) / 6.28318530718 + 0.5,
                               acos(clamp(direction.y, -1.0, 1.0)) / 3.14159265359);
            // 钳制到边缘像素，避免上下极点通过环绕采样互相污染。
            uint width, height;
            skyPanorama.GetDimensions(width, height);
            uv.y = clamp(uv.y, 0.5 / height, 1.0 - 0.5 / height);
            color = skyPanorama.SampleLevel(defaultSampler, uv, 0);
        }
    }
    else
    {
        return float4(EvaluateProceduralSky(direction, Sky, fwidth(dot(direction, Sky.SunAndRadius.xyz))), 1);
    }
    return float4(max(color, 0) * Sky.ZenithAndIntensity.w, 1);
}
