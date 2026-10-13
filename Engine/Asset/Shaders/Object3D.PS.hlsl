#include "Light/Lighting.hlsli"

Texture2D<float4> baseColorTexture : register(t0);
#ifdef USE_METALLICROUGHNESS
Texture2D<float3> metallicRoughnessTexture : register(t1);
Texture2D<float3> roughnessTexture : register(t2);
Texture2D<float3> normalTexture : register(t3);
Texture2D<float1> occlusionTexture : register(t4);
Texture2D<float3> emissiveTexture : register(t5);
#else
Texture2D<float3> metallicTexture : register(t1);
Texture2D<float3> roughnessTexture : register(t2);
Texture2D<float3> normalTexture : register(t3);
Texture2D<float1> occlusionTexture : register(t4);
Texture2D<float3> emissiveTexture : register(t5);
#endif

Texture2D<float3> specularTexture : register(t6);

TextureCube<float3> radianceIBLTexture : register(t10);
TextureCube<float3> irradianceIBLTexture : register(t11);
Texture2D<float2> brdfLUT : register(t12);
Texture2D<float> texSSAO : register(t13);
Texture2D<float> texSunShadow : register(t14);

cbuffer MaterialConstants : register(b0)
{
    float4x4 uvTransform;
    float4 baseColorFactor;
    float3 emissiveFactor;
    float normalTextureScale;
    float3 FresnelF0;
    float2 metallicRoughnessFactor;
    uint useLight;
    float aoFactor;
    uint textureOverrides;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITTANGENT;
    
    float3 worldPos : TEXCOORD1;
    float4 sunShadowCoord : TEXCOORD2;
};

float3 ComputeNormal(VSOutput intput, float2 uv)
{
    float3 normalTS = normalTexture.Sample(defaultSampler, uv) * 2.0 - 1.0;
    float gloss = 128.0;
    AntiAliasSpecular(normalTS, gloss);
    
    float3 N = normalize(intput.normal);
    float3 T = normalize(intput.tangent);
    float3 B = normalize(intput.bitangent);
    float3x3 TBN = float3x3(T, B, N);

    normalTS = normalize(normalTS * float3(normalTextureScale, normalTextureScale, 1));

    return mul(normalTS, TBN);
}

// Diffuse IBL
float3 Diffuse_IBL(float3 diffuseAlbedo, float roughness,float3 N,float3 V)
{
    float3 irradiance = irradianceIBLTexture.Sample(defaultSampler, N).rgb;

    float LdotH = saturate(dot(N, normalize(N + V)));
    float fd90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    float3 diffuseFresnel = Fresnel_Shlick(1.0, fd90, saturate(dot(N, V)));
    return diffuseAlbedo * irradiance * diffuseFresnel;
}
// Specular IBL
float3 Specular_IBL(float3 specularAlbedo, float3 V, float3 N, float roughness)
{
    float3 R = reflect(-V, N);
    float lod = roughness * IBLRange + IBLBias;
    float3 specular = Fresnel_Shlick(specularAlbedo, 1, saturate(dot(N, V)));

    return specular * radianceIBLTexture.SampleLevel(defaultSampler, reflect(-V, N), lod);
}

float4 main(VSOutput input) : SV_Target0
{
    
    float2 uv = mul(float4(input.texcoord, 0.0, 1.0), uvTransform).xy;
    #ifdef USE_METALLICROUGHNESS
    float4 baseColor = baseColorFactor * baseColorTexture.Sample(defaultSampler,uv);
    float3 packed=metallicRoughnessTexture.Sample(defaultSampler,uv);
    float metallic=metallicRoughnessFactor.x*((textureOverrides & 128)?packed.b:((textureOverrides & 2)?packed.r:packed.b));
    float roughness=metallicRoughnessFactor.y*((textureOverrides & 256)?roughnessTexture.Sample(defaultSampler,uv).g:((textureOverrides & 4)?roughnessTexture.Sample(defaultSampler,uv).r:packed.g));
    float occlusion = occlusionTexture.Sample(defaultSampler,uv);
    float3 emissive = emissiveFactor * emissiveTexture.Sample(defaultSampler, uv);
    float3 N = ComputeNormal(input,uv);
    #else
    float4 baseColor = baseColorFactor * baseColorTexture.Sample(defaultSampler, uv);
    float3 metallicSample = metallicTexture.Sample(defaultSampler, uv);
    float metallic = metallicRoughnessFactor.x * ((textureOverrides & 128)?metallicSample.b:metallicSample.r);
    float3 roughnessSample = roughnessTexture.Sample(defaultSampler, uv);
    float roughness = metallicRoughnessFactor.y * ((textureOverrides & 256)?roughnessSample.g:roughnessSample.r);
    float occlusion = occlusionTexture.Sample(defaultSampler, uv);
    float3 emissive = emissiveFactor * emissiveTexture.Sample(defaultSampler, uv);
    float3 N = ComputeNormal(input, uv);
    #endif

    
    float3 V = normalize(CameraPos - input.worldPos);

    occlusion *= aoFactor;
    metallic=saturate(metallic);
    float3 F0 = lerp(FresnelF0 * specularTexture.Sample(defaultSampler,uv), baseColor.rgb, metallic);

    float3 diffuseAlbedo = baseColor.rgb * (1.0 - metallic) * occlusion;

    float3 specularAlbedo = F0 * occlusion;

    roughness = clamp(roughness, 0.045, 1.0);
    input.sunShadowCoord.xyz /= input.sunShadowCoord.w;
    //太陽光(平行光源)
#ifdef MATERIAL_PREVIEW
    float3 dirLight = ApplyLightCommon(diffuseAlbedo,specularAlbedo,roughness,N,V,-SunDirection,SunIntensity);
#else
    float3 dirLight = ApplyDirectionalLight(diffuseAlbedo, specularAlbedo, roughness, N, V, -SunDirection, SunIntensity, input.sunShadowCoord.xyz, texSunShadow);
#endif

    uint2 pixelPos = uint2(input.position.xy);
#ifdef MATERIAL_PREVIEW
    float ssao=1.0;
#else
    float ssao = texSSAO.Load(int3(pixelPos, 0.0));
#endif
    
    //間接光
    //拡散反射
    float3 diffuseIBL = Diffuse_IBL(diffuseAlbedo, roughness, N, V) * IBLFactor;
    //鏡面反射
    float3 specularIBL = Specular_IBL(specularAlbedo, V, N, roughness) * IBLFactor;
    

    diffuseIBL *= ssao;
    specularIBL *= ssao;
    
    //todo: tiled light
    float3 tiledLightColor = 0.0f;
    float3 color = 0.0f;
#ifndef MATERIAL_PREVIEW
    ShadeLights(tiledLightColor, pixelPos, diffuseAlbedo, specularAlbedo, roughness, N, V, input.worldPos);
#endif
    if (useLight == 0)
    {
        dirLight = float3(0, 0, 0);
        tiledLightColor = float3(0, 0, 0);
        color += emissive + baseColor.rgb;
    }
    else
    {
        color += emissive + dirLight + diffuseIBL  + specularIBL  + tiledLightColor;
    }


#ifdef MATERIAL_PREVIEW
    color=pow(max(color,0)/(1+max(color,0)),1.0/2.2);
#endif
    return float4(color, baseColor.a);
}
