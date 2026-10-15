#include "ParticleV2.hlsli"

StructuredBuffer<Particle> particles : register(t0);

struct Output
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};
Output main(uint vertex : SV_VertexID, uint instance : SV_InstanceID)
{
    float2 corners[6] = { float2(-.5, -.5), float2(-.5, .5), float2(.5, -.5), float2(.5, -.5), float2(-.5, .5), float2(.5, .5) };
    Particle p = particles[instance];
    float2 xy = corners[vertex];
    float3 right = billboard ? cameraRight.xyz : float3(1, 0, 0), up = billboard ? cameraUp.xyz : float3(0, 1, 0);
    Output o;
    o.position = mul(float4(p.positionAge.xyz + (xy.x * right + xy.y * up) * p.colorSize.w, 1), viewProj);
    o.uv = xy + 0.5;
    float age = saturate(p.positionAge.w / max(p.velocityLife.w, .0001));
    o.color = float4(p.colorSize.rgb, lerp(p.initialColor.a, p.finalColor.a, age));
    if (p.positionAge.w < 0)
    {
        o.position = float4(0, 0, 0, 1);
        o.color = 0;
    }
    return o;
}
