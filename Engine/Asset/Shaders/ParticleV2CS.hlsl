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
        uint seed = hash(i ^ pad0);
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
        bool varied=controls.w!=0;
        float initialSpeed=varied?lerp(lifeSpeedRanges.z,lifeSpeedRanges.w,random(seed+31)):speed;
        p.velocityLife = float4(d * initialSpeed, varied?lerp(lifeSpeedRanges.x,lifeSpeedRanges.y,random(seed+53)):gravityLife.w);
        p.sizes=float4(varied?lerp(sizeRanges.x,sizeRanges.y,random(seed+71)):controls.x,varied?lerp(sizeRanges.z,sizeRanges.w,random(seed+97)):controls.y,0,0);
        float4 startRandom=float4(random(seed+113),random(seed+131),random(seed+151),random(seed+173));
        float4 endRandom=float4(random(seed+197),random(seed+223),random(seed+251),random(seed+277));
        p.initialColor=varied?lerp(startMin,startMax,startRandom):startColor;
        p.finalColor=varied?lerp(endMin,endMax,endRandom):endColor;
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
    p.colorSize = float4(lerp(p.initialColor.rgb, p.finalColor.rgb, age), lerp(p.sizes.x, p.sizes.y, age));
    particles[i] = p;
}
