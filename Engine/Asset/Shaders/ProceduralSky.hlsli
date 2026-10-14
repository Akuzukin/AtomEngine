#ifndef ATOM_PROCEDURAL_SKY
#define ATOM_PROCEDURAL_SKY
// 背景和 IBL 共用同一套线性 HDR 天空参数，保持色彩、亮度与太阳一致。
struct SkyParameters
{
    float4 ZenithAndIntensity;
    float4 HorizonAndRotation;
    float4 GroundAndMode;
    float4 SunAndRadius;
    float4 SunColorAndStrength;
};
float3 EvaluateProceduralSky(float3 direction, SkyParameters sky, float sunEdge)
{
    float3 color = direction.y >= 0 ? lerp(sky.HorizonAndRotation.rgb, sky.ZenithAndIntensity.rgb, pow(saturate(direction.y), 0.45)) :
        lerp(sky.HorizonAndRotation.rgb, sky.GroundAndMode.rgb, saturate(-direction.y * 6));
    float alignment = dot(direction, sky.SunAndRadius.xyz);
    float radiusCosine = cos(sky.SunAndRadius.w);
    float edge = max(sunEdge, 0.000001);
    float disk = smoothstep(radiusCosine - edge, radiusCosine + edge, alignment);
    float halo = pow(saturate(alignment), 128) * 0.12;
    color += sky.SunColorAndStrength.rgb * sky.SunColorAndStrength.w * (disk + halo) * smoothstep(-0.03, 0.03, direction.y);
    return max(color, 0) * sky.ZenithAndIntensity.w;
}
#endif
