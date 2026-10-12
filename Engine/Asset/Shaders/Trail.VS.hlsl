cbuffer gCamera : register(b0)
{
    float4x4 viewProj;
};

struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float age : TEXCOORD1;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float age : TEXCOORD1;
};

PSInput main(VSInput IN)
{
    PSInput OUT;
    OUT.position = mul(IN.position, viewProj);
    OUT.color = IN.color;
    OUT.uv = IN.uv;
    OUT.age = IN.age;
    return OUT;
}