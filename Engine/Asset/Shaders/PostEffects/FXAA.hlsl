#include "../Utility.hlsli"
Texture2D<float4> Source : register(t0);
SamplerState LinearClamp : register(s0);
cbuffer Constants : register(b0) { float2 InvSize; }
float Luma(float3 c) { return dot(ApplySRGBCurve(max(c,0)),float3(.299,.587,.114)); }
float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float3 center = Source.SampleLevel(LinearClamp,uv,0).rgb;
    float nw = Luma(Source.SampleLevel(LinearClamp,uv + float2(-1,-1)*InvSize,0).rgb);
    float ne = Luma(Source.SampleLevel(LinearClamp,uv + float2( 1,-1)*InvSize,0).rgb);
    float sw = Luma(Source.SampleLevel(LinearClamp,uv + float2(-1, 1)*InvSize,0).rgb);
    float se = Luma(Source.SampleLevel(LinearClamp,uv + float2( 1, 1)*InvSize,0).rgb);
    float mid = Luma(center), low = min(mid,min(min(nw,ne),min(sw,se))), high = max(mid,max(max(nw,ne),max(sw,se)));
    if (high-low < max(.0312,high*.125)) return float4(center,1);
    float2 direction = float2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));
    float reduce = max((nw+ne+sw+se)*(.25*.125),1./128);
    float reciprocal = rcp(min(abs(direction.x),abs(direction.y))+reduce);
    direction = clamp(direction*reciprocal,-8,8)*InvSize;
    float3 a = .5*(Source.SampleLevel(LinearClamp,uv+direction*(1./3-.5),0).rgb + Source.SampleLevel(LinearClamp,uv+direction*(2./3-.5),0).rgb);
    float3 b = a*.5+.25*(Source.SampleLevel(LinearClamp,uv-direction*.5,0).rgb + Source.SampleLevel(LinearClamp,uv+direction*.5,0).rgb);
    float lum = Luma(b);
    return float4(lum < low || lum > high ? a : b,1);
}
