#pragma once
#include "Runtime/Platform/DirectX12/Core/DescriptorHeap.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Function/Framework/ECS/World.h"

namespace AtomEngine
{
	enum CommonTextureSlot { DiffuseIBL, SpecularIBL, BRDFLUT, AmbientOcclusion, SunShadow };

	class ColorBuffer;
	class DepthBuffer;

	enum RootBindings
	{
		kMeshConstants,			// メッシュ定数バッファ
		kMaterialConstants,		// マテリアル定数バッファ
		kMaterialSRVs,			// テクスチャ
		kCommonSRVs,			// 共通テクスチャ(depth 環境mapとか)
		kCommonCBV,				// グローバル定数バッファ
        kLightConstants,		// ライト定数バッファ
		kSkinMatrices,			// スキニング行列バッファ

		kNumRootBindings
	};

	enum class PSOIndex : uint16_t
	{
		kPSO_Default,
		kPSO_Skin,
		kPSO_MRWorkFlow,
		kPSO_MRWorkFlow_Skin,
		kPSO_Transparent,
		kPSO_Transparent_Skin,
		kPSO_Transparent_MRWorkFlow,
        kPSO_Transparent_MRWorkFlow_Skin,
		kPSO_DepthOnly,
		kPSO_DepthOnly_Skin,
		kPSO_Shadow,
        kPSO_Shadow_Skin,

		kNumPSOs
	};

	// Per-draw view contract. History effects currently belong to the primary view.
	struct RenderView
	{
		World& world;
		Camera& camera;
		ColorBuffer& color;
		DepthBuffer& depth;
		D3D12_VIEWPORT viewport;
		D3D12_RECT scissor;
	};

	class Renderer
	{
	public:

		static void Initialize();
		static void InitializeRenderPasses();
		static void Update(World& world,float deltaTime);
		static void UpdateBuffers();
		static void Render(World& world,Camera& camera ,float deltaTime);

		static void Render(const RenderView& view);
		static void Shutdown();

		static DescriptorHeap& GetTextureHeap() { return gTextureHeap; }
        static DescriptorHandle& GetCommonTextures() { return gCommonTextures; }

		static const RootSignature& GetRootSignature() { return mRootSig; }
		static const GraphicsPSO& GetPSO(uint16_t psoFlags);
	private:
		static void RebuildPipelines();
        static std::vector<GraphicsPSO> gPSOs;
		static RootSignature mRootSig;
		static DescriptorHeap gTextureHeap;
		static DescriptorHandle gCommonTextures;
	};
}
