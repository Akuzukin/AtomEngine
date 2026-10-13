#ifndef ConstantBufferStructures_h
#define ConstantBufferStructures_h
#endif

#ifdef __cplusplus
#include "Runtime/Core/Math/MathInclude.h"
using namespace AtomEngine;
using float4 = Vector4;
using float3 = Vector3;
using float2 = Vector2;
using float4x4 = Matrix4x4;
using float3x3 = Matrix3x3;
using uint = uint32_t;
#endif

__declspec(align(256)) struct MeshConstants
{
	float4x4 worldMatrix;              // World matrix
	float4x4 worldInverseTranspose;    // World matrix inverse transpose(法線用)
};

__declspec(align(256)) struct MaterialConstants
{
	Matrix4x4 uvTransform;
	float4 baseColorFactor = { 1,1,1,1 };      // default=[1,1,1,1]
	float3 emissiveFactor = { 1,1,1 };         // default=[1,1,1]
	float normalTextureScale = 1.0f;           // default=1
	float3 FresnelF0 = { 0.04f,0.04f,0.04f };
	float pad0;
	float metallicFactor = 1.0f;               // default=1
	float roughnessFactor = 1.0f;              // default=1
	uint32_t uselight = 1;
    float aoFactor=1;
    uint32_t textureOverrides=0;
};

__declspec(align(256)) struct GlobalConstants
{
	float4x4 ViewProjMatrix;		// ビュー・プロジェクション行列
	float4x4 SunShadowMatrix;		// light space行列
	float3 CameraPos;				// カメラ位置
	float IBLRange;					// IBLの範囲
	float3 SunDirection;			// メイン平行光源の方向
	float IBLBias;					// IBLのバイアス
	float3 SunIntensity;			// メイン平行光源の強度
	float IBLFactor;				// IBLの強度
	float ShadowTexelSize[4];
	float InvTileDim[4];
	uint32_t TileCount[4];
};
