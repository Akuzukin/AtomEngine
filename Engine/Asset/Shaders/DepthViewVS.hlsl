cbuffer VSConstants : register(b0)
{
    float4x4 WVP;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TexCoord0;
};

VSOutput main(VSInput vsInput)
{
    VSOutput vsOutput;
    vsOutput.pos = mul(float4(vsInput.pos, 1.0),WVP);
    vsOutput.uv = vsInput.uv;
    return vsOutput;
}