#include "Utility.hlsli"

Texture2D<float3> MainBuffer : register(t0);
Texture2D<float4> OverlayBuffer : register(t1);

#ifdef ScaledComposite
SamplerState linearClamp : register(s0);

cbuffer Constants : register(b0)
{
    float2 UVOffset;
}

float3 SampleColor(float2 uv)
{
    return MainBuffer.SampleLevel(linearClamp, uv, 0);
}

float3 ScaleBuffer(float2 uv)
{
    return 1.4 * SampleColor(uv) - 0.1 * (
        SampleColor(uv + float2(+UVOffset.x, +UVOffset.y)) +
        SampleColor(uv + float2(+UVOffset.x, -UVOffset.y)) +
        SampleColor(uv + float2(-UVOffset.x, +UVOffset.y)) +
        SampleColor(uv + float2(-UVOffset.x, -UVOffset.y))
        );
}
#endif

float4 main(float4 position : SV_Position, float2 uv : TexCoord0) : SV_Target0
{
#ifdef ScaledComposite
    float3 MainColor = ScaleBuffer(uv);
#else
    float3 MainColor = MainBuffer[(int2) position.xy];
#endif
    float4 OverlayColor = OverlayBuffer[(int2) position.xy];
    float3 output = OverlayColor.rgb + MainColor.rgb * (1.0 - OverlayColor.a);
    return float4(ApplySRGBCurve(max(output,0)), 1.0);
}
