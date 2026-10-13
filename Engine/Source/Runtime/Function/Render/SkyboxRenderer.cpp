#include "SkyboxRenderer.h"
#include "RenderSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Resource/AssetManager.h"

namespace AtomEngine
{
	void SkyboxRenderer::Initialize()
	{
		SamplerDesc DefaultSamplerDesc;
		DefaultSamplerDesc.MaxAnisotropy = 8;

		mSkyboxRootSig.Reset(3, 1);
		mSkyboxRootSig.InitStaticSampler(10, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		mSkyboxRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		mSkyboxRootSig[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);
		mSkyboxRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

		mSkyboxRootSig.Finalize(L"Skybox RootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		DXGI_FORMAT ColorFormat = gSceneColorBuffer.GetFormat();
		DXGI_FORMAT DepthFormat = gSceneDepthBuffer.GetFormat();

		auto skyVS = ShaderCompiler::CompileBlob(L"SkyboxVS.hlsl", L"vs_6_2");
		auto skyPS = ShaderCompiler::CompileBlob(L"SkyboxPS.hlsl", L"ps_6_2");

		mSkyboxPSO.SetRootSignature(mSkyboxRootSig);
		mSkyboxPSO.SetRasterizerState(RasterizerTwoSided);
		mSkyboxPSO.SetBlendState(BlendDisable);
		mSkyboxPSO.SetDepthStencilState(DepthStateReadOnly);
		mSkyboxPSO.SetInputLayout(0, nullptr);
		mSkyboxPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		mSkyboxPSO.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);
		mSkyboxPSO.SetInputLayout(0, nullptr);
		mSkyboxPSO.SetVertexShader(skyVS.Get());
		mSkyboxPSO.SetPixelShader(skyPS.Get());
		mSkyboxPSO.Finalize();

	}

	void SkyboxRenderer::Shutdown()
	{
		mEnvironmentMap = nullptr;
		mRadianceCubeMap = nullptr;
		mIrradianceCubeMap = nullptr;
	}

	void SkyboxRenderer::SetEnvironmentMap(TextureRef environmentMap)
	{
		mEnvironmentMap = environmentMap;
	}

	void SkyboxRenderer::SetIBLTextures(TextureRef diffuseIBL, TextureRef specularIBL)
	{
		mRadianceCubeMap = specularIBL;
		mIrradianceCubeMap = diffuseIBL;

		mSpecularIBLRange = 0.0f;
		if (mRadianceCubeMap.IsValid())
		{
			ID3D12Resource* texRes = const_cast<ID3D12Resource*>(mRadianceCubeMap.Get()->GetResource());
			const D3D12_RESOURCE_DESC& texDesc = texRes->GetDesc();
			mSpecularIBLRange = Math::Max(0.0f, (float)texDesc.MipLevels - 1);
			mSpecularIBLBias = Math::Min(mSpecularIBLBias,mSpecularIBLRange);
		}

		uint32_t DestCount = 3;
		uint32_t SourceCounts[] = { 1, 1, 1};

		D3D12_CPU_DESCRIPTOR_HANDLE SourceTextures[] =
		{
			specularIBL.IsValid() ? specularIBL.GetSRV() : GetDefaultTexture(kBlackCubeMap),
			diffuseIBL.IsValid() ? diffuseIBL.GetSRV() : GetDefaultTexture(kBlackCubeMap),
            mBRDF_LUT.IsValid() ? mBRDF_LUT.GetSRV() : GetDefaultTexture(kDefaultBRDFLUT)
		};
		auto& commonTextures = Renderer::GetCommonTextures();

		DX12Core::gDevice->CopyDescriptors(1, &commonTextures, &DestCount, DestCount,
			SourceTextures, SourceCounts, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	}

	void SkyboxRenderer::SetBRDF_LUT(const std::wstring& BRDF_LUT_File)
	{
        mBRDF_LUT = AssetManager::LoadTextureFile(BRDF_LUT_File);
	}

	void SkyboxRenderer::SetEnvironmentMap(const std::wstring& file)
	{
		mEnvironmentMap = AssetManager::LoadTextureFile(file);
	}

	void SkyboxRenderer::SetIBLTextures(const std::wstring& diffuseFile,const std::wstring& specularFile)
	{
		TextureRef specularIBL = AssetManager::LoadTextureFile(specularFile);
		TextureRef diffuseIBL = AssetManager::LoadTextureFile(diffuseFile);
        SetIBLTextures(diffuseIBL,specularIBL);
	}

	void SkyboxRenderer::SetIBLBias(float LODBias)
	{
		mSpecularIBLBias = std::min(LODBias, mSpecularIBLRange);
	}

	void SkyboxRenderer::Render(GraphicsContext& context, const Camera* camera, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor)
	{
		__declspec(align(16)) struct SkyboxVSCB
		{
			Matrix4x4 ProjInverse;
			Matrix4x4 ViewInverse;
		} skyVSCB;
		skyVSCB.ProjInverse = camera->GetProjMatrix().Inverse();
		skyVSCB.ViewInverse = camera->GetViewMatrix().Inverse();

		__declspec(align(16)) struct SkyboxPSCB
		{
			float TextureLevel;
		} skyPSCB;
		skyPSCB.TextureLevel = mEnvironmentMap.IsValid() ? 0.f : mSpecularIBLBias;

		auto& textureHeap = Renderer::GetTextureHeap();
		auto& commonTextures = Renderer::GetCommonTextures();

		context.SetRootSignature(mSkyboxRootSig);
		context.SetPipelineState(mSkyboxPSO);

		context.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
		context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		context.SetRenderTarget(gSceneColorBuffer.GetRTV(), gSceneDepthBuffer.GetDSV_DepthReadOnly());
		context.SetViewportAndScissor(viewport, scissor);

		context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, textureHeap.GetHeapPointer());
		context.SetDynamicConstantBufferView(kMeshConstants, sizeof(SkyboxVSCB), &skyVSCB);
		context.SetDynamicConstantBufferView(kMaterialConstants, sizeof(SkyboxPSCB), &skyPSCB);
		context.SetDynamicDescriptor(2, 0, mEnvironmentMap.IsValid() ? mEnvironmentMap.GetSRV() :
            mRadianceCubeMap.IsValid() ? mRadianceCubeMap.GetSRV() : GetDefaultTexture(kBlackCubeMap));

		context.Draw(3);

	}

	float SkyboxRenderer::GetIBLRange()const
	{
		return mSpecularIBLRange - mSpecularIBLBias;
	}

	float SkyboxRenderer::GetIBLBias()const
	{
		return mSpecularIBLBias;
	}

}
