#include "Common.hlsli"

cbuffer VSConstants : register(b0)
{
    float4x4 ProjInverse;
    float4x4 ViewInverse;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 viewDir : TEXCOORD3;
};

VSOutput main(uint VertID : SV_VertexID)
{
    float2 ScreenUV = float2(uint2(VertID, VertID << 1) & 2);
    float4 ProjectedPos = float4(lerp(float2(-1, 1), float2(1, -1), ScreenUV), 0, 1);
    // Unproject the near plane to avoid w=0 with an infinite reverse-Z far plane.
    float4 PosViewSpace = mul(float4(ProjectedPos.xy, 1, 1), ProjInverse);

    VSOutput vsOutput;
    vsOutput.position = ProjectedPos;
    vsOutput.viewDir = mul(PosViewSpace.xyz, (float3x3)ViewInverse);

    return vsOutput;
}
