#include "Utility.hlsli"
// The SRGB SRV decodes the hardware-encoded, premultiplied-linear ImGui target.
// The editor's destination was cleared to black after capturing the scene preview.
Texture2D<float4> UiBuffer : register(t0);
float4 main(float4 position : SV_Position) : SV_Target0
{
    return float4(ApplySRGBCurve(UiBuffer[int2(position.xy)].rgb),1.0);
}
