#include "ProceduralSky.hlsli"
cbuffer Constants : register(b0)
{
    SkyParameters Sky;
    float4 Filter; // 分辨率、粗糙度、类型（0 原始天空/1 漫反射/2 GGX 镜面）、保留。
};
RWTexture2DArray<float4> Output : register(u0);

float3 CubeDirection(uint face, float2 uv)
{
    float2 p = uv * 2 - 1;
    if (face == 0)
        return normalize(float3(1, -p.y, -p.x));
    if (face == 1)
        return normalize(float3(-1, -p.y, p.x));
    if (face == 2)
        return normalize(float3(p.x, 1, p.y));
    if (face == 3)
        return normalize(float3(p.x, -1, -p.y));
    if (face == 4)
        return normalize(float3(p.x, -p.y, 1));
    return normalize(float3(-p.x, -p.y, -1));
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= (uint) Filter.x || id.y >= (uint) Filter.x || id.z >= 6)
        return;
    float3 n = CubeDirection(id.z, (id.xy + 0.5) / Filter.x);
    float3 color = 0;
    if (Filter.z < 0.5)
        color = EvaluateProceduralSky(n, Sky, 0.000001);
    else
    {
        float3 up = abs(n.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, n));
        float3 bitangent = cross(n, tangent);
        float totalWeight = 0;
        const uint samples = 128;
        for (uint i = 0; i < samples; ++i)
        {
            float2 xi = float2((i + 0.5) / samples, reversebits(i) * 2.3283064365386963e-10);
            float phi = 6.28318530718 * xi.x;
            float cosTheta;
            if (Filter.z < 1.5)
                cosTheta = sqrt(1 - xi.y); // 余弦加权半球；平均值即 irradiance / PI。
            else
            {
                float alpha = Filter.y * Filter.y;
                cosTheta = sqrt((1 - xi.y) / (1 + (alpha * alpha - 1) * xi.y));
            }
            float sinTheta = sqrt(max(0, 1 - cosTheta * cosTheta));
            float3 h = tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + n * cosTheta;
            float3 l = Filter.z < 1.5 ? h : normalize(2 * dot(n, h) * h - n);
            float weight = Filter.z < 1.5 ? 1 : saturate(dot(n, l));
            color += EvaluateProceduralSky(l, Sky, 0.000001) * weight;
            totalWeight += weight;
        }
        color /= max(totalWeight, 0.000001);
    }
    Output[id] = float4(color, 1);
}
