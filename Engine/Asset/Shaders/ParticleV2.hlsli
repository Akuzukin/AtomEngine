struct Particle
{
    float4 positionAge;
    float4 velocityLife;
    float4 colorSize;
};
cbuffer Params : register(b0)
{
    row_major float4x4 viewProj;
    float4 cameraRight, cameraUp, originDt, gravityLife, startColor, endColor, extentRadius, controls;
    uint capacity, spawnStart, spawnCount, shape;
    float speed, drag, cone, time;
    uint billboard, textured, pad0, pad1;
};
