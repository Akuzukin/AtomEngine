#include "ParticleV2.hlsli"
RWStructuredBuffer<Particle> particles : register(u0);
uint hash(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    return x ^ (x >> 16);
}
float random(uint x)
{
    return (hash(x) & 0xffffff) / 16777216.0;
}
[numthreads(64, 1, 1)]void main(uint3 thread : SV_DispatchThreadID)
{
    uint i = thread.x;
    if (i >= capacity)
        return;
    Particle p = particles[i];
    uint offset = (i + capacity - spawnStart) % capacity;
    if (offset < spawnCount)
    {
        uint seed = i + asuint(time);
        float a = random(seed) * 6.2831853, z = random(seed + 17) * 2 - 1;
        float3 d = float3(sqrt(max(0, 1 - z * z)) * cos(a), z, sqrt(max(0, 1 - z * z)) * sin(a));
        float3 position = 0;
        if (shape == 1)
            position = float3(random(seed + 3) * 2 - 1, random(seed + 4) * 2 - 1, random(seed + 5) * 2 - 1) * extentRadius.xyz;
        if (shape == 2)
            position = d * extentRadius.w * pow(random(seed + 6), .333333);
        if (shape == 3)
        {
            float y = lerp(cos(radians(cone)), 1, random(seed + 8));
            float r = sqrt(max(0, 1 - y * y));
            d = float3(cos(a) * r, y, sin(a) * r);
        }
        if (shape == 4)
        {
            d = float3(cos(a), 0, sin(a));
            position = d * extentRadius.w;
        }
        p.positionAge = float4(originDt.xyz + position, 0);
        p.velocityLife = float4(d * speed, gravityLife.w);
    }
    else if (p.positionAge.w >= 0)
    {
        p.positionAge.w += originDt.w;
        if (p.positionAge.w >= p.velocityLife.w)
            p.positionAge.w = -1;
        else
        {
            p.velocityLife.xyz = (p.velocityLife.xyz + gravityLife.xyz * originDt.w) * exp(-drag * originDt.w);
            p.positionAge.xyz += p.velocityLife.xyz * originDt.w;
        }
    }
    float age = saturate(p.positionAge.w / max(p.velocityLife.w, .0001));
    p.colorSize = float4(lerp(startColor.rgb, endColor.rgb, age), lerp(controls.x, controls.y, age));
    particles[i] = p;
}
