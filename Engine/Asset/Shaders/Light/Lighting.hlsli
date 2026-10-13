#include"../Common.hlsli"
#include"LightGrid.hlsli"

cbuffer GlobalConstants : register(b1)
{
    float4x4 ViewProjMatrix;
    float4x4 SunShadowMatrix;
    float3 CameraPos;
    float IBLRange;
    float3 SunDirection;
    float IBLBias;
    float3 SunIntensity;
    float IBLFactor;
    float4 ShadowTexelSize;
    float4 InvTileDim;
    uint4 TileCount;
}

StructuredBuffer<LightData> lightBuffer : register(t15);
Texture2DArray<float> lightShadowArrayTex : register(t16);
ByteAddressBuffer lightGrid : register(t17);
ByteAddressBuffer lightGridBitMask : register(t18);

float ComputeNormalBias(float3 normal, float3 lightDir, float baseBias)
{
    float NoL = saturate(dot(normal, lightDir));
    return baseBias * (1.0 - NoL);
}

void AntiAliasSpecular(inout float3 texNormal, inout float gloss)
{
    float normalLenSq = dot(texNormal, texNormal);
    float invNormalLen = rsqrt(normalLenSq);
    texNormal *= invNormalLen;
    float normalLen = normalLenSq * invNormalLen;
    float flatness = saturate(1 - abs(ddx(normalLen)) - abs(ddy(normalLen)));
    gloss = exp2(lerp(0, log2(gloss), flatness));
}

// Apply fresnel to modulate the specular albedo
void FSchlick(inout float3 specular, inout float3 diffuse, float3 lightDir, float3 halfVec)
{
    float fresnel = pow(1.0 - saturate(dot(lightDir, halfVec)), 5.0);
    specular = lerp(specular, 1, fresnel);
    diffuse = lerp(diffuse, 0, fresnel);
}

float3 ApplyAmbientLight(
    float3 diffuse, // Diffuse albedo
    float ao, // Pre-computed ambient-occlusion
    float3 lightColor // Radiance of ambient light
    )
{
    return ao * diffuse * lightColor;
}

//float GetDirectionalShadow(float3 ShadowCoord, Texture2D<float> texShadow)
//{
//    const float Dilation = 2.0;
//    float d1 = Dilation * ShadowTexelSize.x * 0.125;
//    float d2 = Dilation * ShadowTexelSize.x * 0.875;
//    float d3 = Dilation * ShadowTexelSize.x * 0.625;
//    float d4 = Dilation * ShadowTexelSize.x * 0.375;
//    float result = (
//        2.0 * texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy, ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
//        texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
//        ) / 10.0;
    
//    return result * result;
//}

float GetDirectionalShadow(float3 ShadowCoord, Texture2D<float> texShadow)
{
    if(any(ShadowCoord<0) || any(ShadowCoord>1))return 1.0;
    float sum = 0.0f;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * ShadowTexelSize.xy;
            sum += texShadow.SampleCmpLevelZero(shadowSampler, ShadowCoord.xy + offset, ShadowCoord.z);
        }
    }
    return sum / 9.0f;
}

float GetShadowConeLight(uint lightIndex, float3 shadowCoord)
{
    float result = lightShadowArrayTex.SampleCmpLevelZero(
        shadowSampler, float3(shadowCoord.xy, lightIndex), shadowCoord.z);
    return result * result;
}

float3 ApplyLightCommon(
    float3 diffuseColor,    // Diffuse albedo
    float3 specularColor,   // F0
    float roughness,        // roughness
    float3 normal,
    float3 viewDir,
    float3 lightDir,
    float3 lightColor
    )
{
    float3 L = normalize(lightDir);
    float3 H = normalize(L + viewDir);

    //dot
    float NdotV = saturate(dot(normal, viewDir));
    float NdotL = saturate(dot(normal, L));
    float NdotH = saturate(dot(normal, H));
    float LdotH = saturate(dot(L, H));

    float alpha = roughness * roughness;
    float alphaSqr = alpha * alpha;
    //Specular: D * G * F
    float D = D_GGX(NdotH, alphaSqr);
    //Schlick-GGX geometric
    float G = G_Smith(NdotV, NdotL, roughness);
    //Fresnel
    float3 F = Fresnel_Shlick(specularColor, LdotH);
    
    //Diffuse Burley
    float3 diffuse = Diffuse_Burley(diffuseColor, roughness, NdotL, NdotV, LdotH);
    float3 specular = (D * G) * F;
    
    return NdotL * lightColor * (diffuse + specular);
}

float3 ApplyDirectionalLight(
    float3 diffuseColor, // Diffuse albedo
    float3 specularColor, // Specular albedo
    float roughness,
    float3 normal, // World-space normal
    float3 viewDir, // World-space vector from eye to point
    float3 lightDir, // World-space vector from point to light
    float3 lightColor, // Radiance of directional light
    float3 shadowCoord, // Shadow coordinate (Shadow map UV & light-relative Z)
	Texture2D<float> texShadow
)
{
    //shadowCoord.z += ComputeNormalBias(normal, lightDir, ShadowBias);
    //shadowCoord.z = floor(shadowCoord.z * 65535.0) / 65535.0;
    float shadow = GetDirectionalShadow(shadowCoord, texShadow);

    return shadow * ApplyLightCommon(
        diffuseColor,
        specularColor,
        roughness,
        normal,
        viewDir,
        lightDir,
        lightColor
        );
}

float3 ApplyPointLight(
    float3 diffuseColor, // Diffuse albedo
    float3 specularColor, // Specular albedo
    float roughness,
    float3 normal, // World-space normal
    float3 viewDir, // World-space vector from eye to point
    float3 worldPos, // World-space fragment position
    float3 lightPos, // World-space light position
    float lightRadiusSq,
    float3 lightColor // Radiance of directional light
    )
{
    float3 lightDir = lightPos - worldPos;
    float lightDistSq = dot(lightDir, lightDir);
    float invLightDist = rsqrt(lightDistSq);
    lightDir *= invLightDist;

    // modify 1/d^2 * R^2 to fall off at a fixed radius
    // (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
    float distanceFalloff = lightRadiusSq * (invLightDist * invLightDist);
    distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));

    return distanceFalloff * ApplyLightCommon(
        diffuseColor,
        specularColor,
        roughness,
        normal,
        viewDir,
        lightDir,
        lightColor
        );
}

float3 ApplyConeLight(
    float3 diffuseColor, // Diffuse albedo
    float3 specularColor, // Specular albedo
    float roughness,
    float3 normal,
    float3 viewDir, // World-space vector from eye to point
    float3 worldPos, // World-space fragment position
    float3 lightPos, // World-space light position
    float lightRadiusSq,
    float3 lightColor, // Radiance of directional light
    float3 coneDir,
    float innerCos,
    float outerCos
    )
{
    float3 lightDir = lightPos - worldPos;
    float lightDistSq = dot(lightDir, lightDir);
    float invLightDist = rsqrt(lightDistSq);
    lightDir *= invLightDist;

    // modify 1/d^2 * R^2 to fall off at a fixed radius
    // (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
    float distanceFalloff = lightRadiusSq * (invLightDist * invLightDist);
    distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));

    float coneFalloff = dot(-lightDir, coneDir);
    coneFalloff = saturate((coneFalloff - outerCos) * innerCos);

    return (coneFalloff * distanceFalloff) * ApplyLightCommon(
        diffuseColor,
        specularColor,
        roughness,
        normal,
        viewDir,
        lightDir,
        lightColor
        );
}

float3 ApplyConeShadowedLight(
    float3 diffuseColor, // Diffuse albedo
    float3 specularColor, // Specular albedo
    float roughness, // roughness
    float3 normal,
    float3 viewDir, // World-space vector from eye to point
    float3 worldPos, // World-space fragment position
    float3 lightPos, // World-space light position
    float lightRadiusSq,
    float3 lightColor, // Radiance of directional light
    float3 coneDir,
    float  innerCos,
    float  outerCos,
    float4x4 shadowTextureMatrix,
    uint lightIndex
    )
{
    float4 shadowCoord = mul(float4(worldPos, 1.0),shadowTextureMatrix);
    shadowCoord.xyz *= rcp(shadowCoord.w);
    float shadow = GetShadowConeLight(lightIndex, shadowCoord.xyz);

    return shadow * ApplyConeLight(
        diffuseColor,
        specularColor,
        roughness,
        normal,
        viewDir,
        worldPos,
        lightPos,
        lightRadiusSq,
        lightColor,
        coneDir,
        innerCos,
        outerCos
        );
}

#define POINT_LIGHT_GROUPS			1
#define SPOT_LIGHT_GROUPS			2
#define SHADOWED_SPOT_LIGHT_GROUPS	1
#define POINT_LIGHT_GROUPS_TAIL			POINT_LIGHT_GROUPS
#define SPOT_LIGHT_GROUPS_TAIL				POINT_LIGHT_GROUPS_TAIL + SPOT_LIGHT_GROUPS
#define SHADOWED_SPOT_LIGHT_GROUPS_TAIL	SPOT_LIGHT_GROUPS_TAIL + SHADOWED_SPOT_LIGHT_GROUPS

uint GetGroupBits(uint groupIndex, uint tileIndex, uint lightBitMaskGroups[4])
{
    return lightBitMaskGroups[groupIndex];
}

uint WaveOr(uint mask)
{
    return WaveActiveBitOr(mask);
}

// Helper function for iterating over a sparse list of bits.  Gets the offset of the next
// set bit, clears it, and returns the offset.
uint PullNextBit(inout uint bits)
{
    uint bitIndex = firstbitlow(bits);
    bits ^= 1u << bitIndex;
    return bitIndex;
}

void ShadeLights(inout float3 colorSum, uint2 pixelPos,
	float3 diffuseAlbedo, // Diffuse albedo
	float3 specularAlbedo, // Specular albedo
	float roughness,
    float3 normal,
	float3 viewDir,
	float3 worldPos
	)
{
    uint2 tilePos = GetTilePos(pixelPos, InvTileDim.xy);
    uint tileIndex = GetTileIndex(tilePos, TileCount.x);
    uint tileOffset = GetTileOffset(tileIndex);

    // Light Grid Preloading setup
    uint lightBitMaskGroups[4] = { 0, 0, 0, 0 };
    uint4 lightBitMask = lightGridBitMask.Load4(tileIndex * 16);
    
    lightBitMaskGroups[0] = lightBitMask.x;
    lightBitMaskGroups[1] = lightBitMask.y;
    lightBitMaskGroups[2] = lightBitMask.z;
    lightBitMaskGroups[3] = lightBitMask.w;
    
#define POINT_LIGHT_ARGS \
    diffuseAlbedo, \
    specularAlbedo, \
    roughness, \
    normal, \
    viewDir, \
    worldPos, \
    lightData.position, \
    lightData.radiusSq, \
    lightData.color

#define CONE_LIGHT_ARGS \
    POINT_LIGHT_ARGS, \
    lightData.direction, \
    lightData.innerCos, \
    lightData.outerCos

#define SHADOWED_LIGHT_ARGS \
    CONE_LIGHT_ARGS, \
    lightData.shadowMatrix, \
    lightIndex
    uint pointLightGroupTail = POINT_LIGHT_GROUPS_TAIL;
    uint spotLightGroupTail = SPOT_LIGHT_GROUPS_TAIL;
    uint spotShadowLightGroupTail = SHADOWED_SPOT_LIGHT_GROUPS_TAIL;

    uint groupBitsMasks[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; i++)
    {
        // combine across threads
        groupBitsMasks[i] = WaveOr(GetGroupBits(i, tileIndex, lightBitMaskGroups));
    }

    uint groupIndex;

    for (groupIndex = 0; groupIndex < pointLightGroupTail; groupIndex++)
    {
        uint groupBits = groupBitsMasks[groupIndex];

        while (groupBits != 0)
        {
            uint bitIndex = PullNextBit(groupBits);
            uint lightIndex = 32 * groupIndex + bitIndex;

            // sphere
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyPointLight(POINT_LIGHT_ARGS);
        }
    }

    for (groupIndex = pointLightGroupTail; groupIndex < spotLightGroupTail; groupIndex++)
    {
        uint groupBits = groupBitsMasks[groupIndex];

        while (groupBits != 0)
        {
            uint bitIndex = PullNextBit(groupBits);
            uint lightIndex = 32 * groupIndex + bitIndex;

            // cone
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeLight(CONE_LIGHT_ARGS);
        }
    }

    for (groupIndex = spotLightGroupTail; groupIndex < spotShadowLightGroupTail; groupIndex++)
    {
        uint groupBits = groupBitsMasks[groupIndex];

        while (groupBits != 0)
        {
            uint bitIndex = PullNextBit(groupBits);
            uint lightIndex = 32 * groupIndex + bitIndex;

            // cone w/ shadow map
            LightData lightData = lightBuffer[lightIndex];
            colorSum += ApplyConeShadowedLight(SHADOWED_LIGHT_ARGS);
        }
    }
    
}
