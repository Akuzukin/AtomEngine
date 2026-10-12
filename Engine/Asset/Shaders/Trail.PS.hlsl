cbuffer gMaterial : register(b0)
{
    float maxAge;
    float time;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float age : TEXCOORD1;
};

float hash(float n)
{
    return frac(sin(n) * 43758.5453123);
}

float noise(float2 x)
{
    float2 i = floor(x);
    float2 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = i.x + i.y * 57.0;
    return lerp(lerp(hash(n + 0.0), hash(n + 1.0), f.x),
                lerp(hash(n + 57.0), hash(n + 58.0), f.x), f.y);
}

float2 fade(float2 t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float2 gradient(float2 p)
{
    float h = hash(p) * 4.0;
    float2 grad = float2(cos(h), sin(h));
    return grad;
}

float perlin(float2 p)
{
    float2 pi = floor(p);
    float2 pf = p - pi;

    float2 g00 = gradient(pi + float2(0, 0));
    float2 g10 = gradient(pi + float2(1, 0));
    float2 g01 = gradient(pi + float2(0, 1));
    float2 g11 = gradient(pi + float2(1, 1));

    float d00 = dot(g00, pf - float2(0, 0));
    float d10 = dot(g10, pf - float2(1, 0));
    float d01 = dot(g01, pf - float2(0, 1));
    float d11 = dot(g11, pf - float2(1, 1));

    float2 f = fade(pf);

    float nx0 = lerp(d00, d10, f.x);
    float nx1 = lerp(d01, d11, f.x);
    float n = lerp(nx0, nx1, f.y);

    return n;
}

float4 main(PSInput IN) : SV_TARGET
{
    float t = saturate(IN.age / maxAge);
    
    float2 noiseUV = IN.uv * float2(3.0, 1.0) + float2(0, -time * 2.0);
    float n = perlin(noiseUV * 8.0);
    float distortion = n * 0.1;

    float distortedX = IN.uv.x + (distortion - 0.1);

    float distToCenter = abs(distortedX - 0.5f) * 2.0f;
    
    float noiseValue = noise(IN.uv * 10.0 - float2(0, time * 5.0));
    float edgeThreshold = 0.4 + (0.7 * noiseValue);
    
    float shape = 1.0 - smoothstep(edgeThreshold - 0.1, edgeThreshold + 0.2, distToCenter);

    float baseAlpha = (1.0f - t);
    float flicker = 0.8f + 0.2f * sin(time * 15.0);

    float alpha = baseAlpha * shape * flicker;

    float3 finalColor = IN.color.rgb;
    if (shape > 0.8)
        finalColor += float3(0.5, 0.5, 0.5) * (shape - 0.8);

    return float4(finalColor * alpha, baseAlpha * IN.color.a);
}