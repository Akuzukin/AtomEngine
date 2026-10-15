#include "SpriteRenderer.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"

namespace AtomEngine
{
	std::vector<SpriteVertex>SpriteRenderer::sVertices;
	std::vector<uint16_t>SpriteRenderer::sIndices;

	std::vector<SpriteRenderer::SpriteItem>SpriteRenderer::sSpriteItems;

	RootSignature SpriteRenderer::sRootSig;
	GraphicsPSO SpriteRenderer::sPSO[kNumBlendModes];

	

	static uint64_t BuildSortKey(const SpriteDesc& desc, BlendMode blendMode,const TextureRef& texture)
	{
		uint64_t key = 0;

		// DepthLayer
		uint64_t layer = static_cast<uint64_t>(desc.depth);
		key |= (layer & 0xFF) << 56;
		// BlendMode
		key |= (static_cast<uint64_t>(blendMode) & 0xFF) << 48;
		// Texture
		uint64_t texID = reinterpret_cast<uint64_t>(texture.Get());
		key |= (texID & 0xFFFFFFFF) << 16;
		// SpriteID
		static uint16_t spriteCounter = 0;
		key |= spriteCounter++;

		return key;
	}


	void SpriteRenderer::Initialize()
	{
		SamplerDesc spriteSamplerDesc;
		sRootSig.Reset(2, 1);
		sRootSig.InitStaticSampler(0, spriteSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);

		sRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
		sRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		sRootSig.Finalize(L"Sprite RootSig",
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		D3D12_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			 D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		auto vs = ShaderCompiler::CompileBlob(L"SpriteVS.hlsl", L"vs_6_2");
		auto ps = ShaderCompiler::CompileBlob(L"SpritePS.hlsl", L"ps_6_2");

		for (uint32_t i = 0; i < kNumBlendModes; i++)
		{
			auto& pso = sPSO[i];

			pso.SetRootSignature(sRootSig);
			pso.SetRasterizerState(RasterizerDefaultCw);
			pso.SetBlendState(gBlendModeTable[BlendMode(i)]);
			pso.SetDepthStencilState(DepthStateDisabled);
			pso.SetInputLayout(_countof(layout), layout);
			pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			pso.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN);
			pso.SetVertexShader(vs.Get());
			pso.SetPixelShader(ps.Get());
			pso.Finalize();
		}
	}

	void SpriteRenderer::Shutdown()
	{
		sVertices.clear();
		sIndices.clear();
		sSpriteItems.clear();
	}

	void SpriteRenderer::AddSprite(const Sprite* sprite,
		const TextureRef& texture,
		BlendMode blendMode)
	{
		if (!sprite || !sprite->Valid())return;

		SpriteItem item;
		item.sprite = sprite;
		item.textureHandle = texture.GetSRV();
		item.blendMode = blendMode;
		item.sortKey = BuildSortKey(sprite->GetDesc(), blendMode, texture);

		sSpriteItems.push_back(item);
	}

	void SpriteRenderer::Sort()
	{
		std::sort(sSpriteItems.begin(), sSpriteItems.end(),
			[](const SpriteItem& a, const SpriteItem& b)->bool
			{
				return a.sortKey < b.sortKey;
			});
	}

	void SpriteRenderer::Render(GraphicsContext& gfxContext)
	{
		if (sSpriteItems.empty())
			return;

		Sort();

		gfxContext.SetRootSignature(sRootSig);

		sVertices.clear();
		sIndices.clear();

		uint32_t baseVertex = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE currentTexture{};
		BlendMode currentBlend = BlendMode::kBlendNone;
		SpriteConstants currentConst{};

		auto flushBatch = [&](
			D3D12_CPU_DESCRIPTOR_HANDLE texture, 
			BlendMode blendMode, 
			const SpriteConstants& constants)
			{
				if (sIndices.empty()) return;

				gfxContext.SetPipelineState(sPSO[uint32_t(blendMode)]);

				gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				gfxContext.SetDynamicVB(0, sVertices.size(), sizeof(SpriteVertex), sVertices.data());
				gfxContext.SetDynamicIB(sIndices.size(), sIndices.data());

				gfxContext.SetDynamicConstantBufferView(0, sizeof(constants), &constants);
				gfxContext.SetDynamicDescriptor(1,0, texture);
				gfxContext.DrawIndexedInstanced((uint32_t)sIndices.size(), 1, 0, 0, 0);

				sVertices.clear();
				sIndices.clear();
				baseVertex = 0;
			};

		Matrix4x4 viewProj = Math::MakeOrthographicProjectionMatrix(
            0.0f, (float)gOverlayBuffer.GetWidth(),
            (float)gOverlayBuffer.GetHeight(), 0.0f,
			0.0f, 1.0f
		);

		for (size_t i = 0; i < sSpriteItems.size(); ++i)
		{
			const SpriteItem& item = sSpriteItems[i];
			const Sprite& sp = *item.sprite;
			auto& desc = sp.GetDesc();

			D3D12_CPU_DESCRIPTOR_HANDLE spriteTex = item.textureHandle;
			BlendMode spriteBlend = item.blendMode;

			SpriteConstants constants{};
			constants.viewProj = viewProj;
			constants.uvTransform = sp.UVMatrix();
			constants.color = desc.color;

			bool first = (i == 0);
			bool textureChanged = (!first && spriteTex.ptr != currentTexture.ptr);
			bool blendChanged = (!first && spriteBlend != currentBlend);

			if (!first && (textureChanged || blendChanged))
			{
				flushBatch(currentTexture, currentBlend, currentConst);
			}

			currentTexture = spriteTex;
			currentBlend = spriteBlend;
			currentConst = constants;

			Vector2 size = desc.size;
			Vector2 pos = desc.position;
			Vector2 pivot = desc.pivot;

			float px = pos.x - size.x * pivot.x;
			float py = pos.y - size.y * pivot.y;

			Vector3 v0 = { px,          py,          0 };
			Vector3 v1 = { px + size.x, py,          0 };
			Vector3 v2 = { px + size.x, py + size.y, 0 };
			Vector3 v3 = { px,          py + size.y, 0 };

			v0 = Math::Transform(v0, sp.WorldMatrix());
			v1 = Math::Transform(v1, sp.WorldMatrix());
			v2 = Math::Transform(v2, sp.WorldMatrix());
			v3 = Math::Transform(v3, sp.WorldMatrix());

			sVertices.push_back({ v0, Vector2{ 0, 0 } });
			sVertices.push_back({ v1, Vector2{ 1, 0 } });
			sVertices.push_back({ v2, Vector2{ 1, 1 } });
			sVertices.push_back({ v3, Vector2{ 0, 1 } });

			uint16_t i0 = baseVertex + 0;
			uint16_t i1 = baseVertex + 1;
			uint16_t i2 = baseVertex + 2;
			uint16_t i3 = baseVertex + 3;

			sIndices.push_back(i0); sIndices.push_back(i1); sIndices.push_back(i2);
			sIndices.push_back(i2); sIndices.push_back(i3); sIndices.push_back(i0);

			baseVertex += 4;
		}

		flushBatch(currentTexture, currentBlend, currentConst);

		sSpriteItems.clear();
	}
}
