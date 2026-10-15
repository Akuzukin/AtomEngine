#include "ParticleV2.hlsli"
Texture2D image : register(t1);
SamplerState linearSampler : register(s0);
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD, float4 color : COLOR) : SV_Target
{
    float4 result = color * (textured ? image.Sample(linearSampler, uv) : float4(1, 1, 1, 1));
    if (controls.z == 2) result.rgb *= result.a;
    return result;
}
