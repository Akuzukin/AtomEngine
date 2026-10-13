Texture2D<float> mask : register(t0);
struct Output
{
    float4 position : SV_POSITION;
};
Output VS(uint id : SV_VertexID)
{
    Output o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float4 PS(Output input) : SV_Target
{
    uint width, height;
    mask.GetDimensions(width, height);
    int2 p = int2(input.position.xy);
    float center = mask.Load(int3(p, 0)), edge = 0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x)
            edge = max(edge, mask.Load(int3(clamp(p + int2(x, y), int2(0, 0), int2(width - 1, height - 1)), 0)));
    return float4(1, 0.5, 0.08, saturate(edge - center));
}
