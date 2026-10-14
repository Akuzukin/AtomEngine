#include "SkyboxRenderer.h"
#include "RenderSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Resource/AssetManager.h"
#include <algorithm>
#include <filesystem>
#include <cwctype>
#include <cstring>
#include "Runtime/Platform/DirectX12/Context/ComputeContext.h"
#include "Runtime/Function/Light/LightManager.h"

namespace AtomEngine
{
	void SkyboxRenderer::Initialize()
	{
        SetSkySettings(SceneSettings{});
        mLightingTextures = Renderer::GetTextureHeap().Alloc(9);
		SamplerDesc DefaultSamplerDesc;
		DefaultSamplerDesc.MaxAnisotropy = 8;

		mSkyboxRootSig.Reset(3, 1);
		mSkyboxRootSig.InitStaticSampler(10, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		mSkyboxRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		mSkyboxRootSig[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);
		mSkyboxRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2, D3D12_SHADER_VISIBILITY_PIXEL);

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
        mLightingRootSig.Reset(2, 0);
        mLightingRootSig[0].InitAsConstantBuffer(0);
        mLightingRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
        mLightingRootSig.Finalize(L"Procedural sky IBL");
        auto lightingCS = ShaderCompiler::CompileBlob(L"ProceduralSkyIBLCS.hlsl", L"cs_6_2");
        mLightingPSO.SetRootSignature(mLightingRootSig);
        mLightingPSO.SetComputeShader(lightingCS.Get());
        mLightingPSO.Finalize();

	}

	void SkyboxRenderer::Shutdown()
	{
        if (!mLightingTextures.IsNull()) Renderer::GetTextureHeap().Free(mLightingTextures);
        mLightingTextures = DescriptorHandle{};
		mEnvironmentMap = nullptr;
		mRadianceCubeMap = nullptr;
		mIrradianceCubeMap = nullptr;
        mProceduralRadiance.Destroy();
        mProceduralIrradiance.Destroy();
        mLightingReady = false; mBoundLighting = -1;
	}

	void SkyboxRenderer::SetEnvironmentMap(TextureRef environmentMap)
	{
		mEnvironmentMap = environmentMap;
        mTextureError.clear();
        if (!mEnvironmentMap.IsValid()) mTextureError = "Sky texture unavailable; using procedural sky";
        else if (auto dimension = mEnvironmentMap->GetSRVDimension(); dimension != D3D12_SRV_DIMENSION_TEXTURECUBE && dimension != D3D12_SRV_DIMENSION_TEXTURE2D)
        {
            mEnvironmentMap = nullptr;
            mTextureError = "Sky requires a single cubemap or 2D panorama; using procedural sky";
        }
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

        mBoundLighting = -1;

	}

	void SkyboxRenderer::SetBRDF_LUT(const std::wstring& BRDF_LUT_File)
	{
        mBRDF_LUT = BRDF_LUT_File.empty() ? TextureRef{} : AssetManager::LoadTextureFile(BRDF_LUT_File);
        mBoundLighting = -1;
	}

	void SkyboxRenderer::SetEnvironmentMap(const std::wstring& file)
	{
        try
        {
            if (file.empty()) { SetEnvironmentMap(TextureRef{}); return; }
            auto extension = std::filesystem::path(file).extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
            SetEnvironmentMap(extension == L".dds" ? AssetManager::LoadTextureFile(file, kBlackCubeMap, false) :
                AssetManager::LoadCovertTexture(file, kBlackOpaque2D, extension != L".hdr" && extension != L".exr"));
        }
        catch (const std::exception &error)
        {
            mEnvironmentMap = nullptr;
            mTextureError = std::string("Sky texture unavailable; using procedural sky: ") + error.what();
        }
	}

    void SkyboxRenderer::SetSkySettings(const SceneSettings &settings, Vector3 sunDirection, Vector3 sunColor)
    {
        mSettings = settings;
        mSkySunDirection = sunDirection.Length() > .0001f ? sunDirection.NormalizedCopy() : Vector3::UP;
        mSkySunColor = sunColor;
    }

    SkyboxRenderer::SkyConstants SkyboxRenderer::Constants() const
    {
        SkyConstants value{};
        static_assert(sizeof(SkyConstants) == 80);
        auto fill = [](float *out, const Vector3 &color, float fourth) { std::copy_n(color.ptr(), 3, out); out[3] = fourth; };
        float mode = 0;
        if (mSettings.skyMode == 1 && mEnvironmentMap.IsValid())
            mode = mEnvironmentMap->GetSRVDimension() == D3D12_SRV_DIMENSION_TEXTURECUBE ? 1.f : 2.f;
        fill(value.zenith, mSettings.skyZenith, mSettings.skyIntensity);
        fill(value.horizon, mSettings.skyHorizon, mSettings.skyRotation * Math::PI / 180.f);
        fill(value.ground, mSettings.skyGround, mode);
        fill(value.sun, mSkySunDirection, mSettings.skySunSize * Math::PI / 360.f);
        fill(value.sunColor, mSkySunColor, mSettings.skySunIntensity);
        return value;
    }

    void SkyboxRenderer::UpdateEnvironmentLighting(GraphicsContext &context)
    {
        bool procedural = ProceduralLighting();
        auto parameters = Constants();
        if (procedural && (!mLightingReady || std::memcmp(&parameters, &mBakedConstants, sizeof(parameters)) != 0))
        {
            if (mProceduralRadiance.GetWidth() == 0)
            {
                mProceduralRadiance.CreateCube(L"Procedural sky GGX radiance", 128, 8, DXGI_FORMAT_R16G16B16A16_FLOAT);
                mProceduralIrradiance.CreateCube(L"Procedural sky diffuse irradiance", 16, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
            }
            auto &compute = context.GetComputeContext();
            compute.SetRootSignature(mLightingRootSig);
            compute.SetPipelineState(mLightingPSO);
            struct alignas(16) FilterConstants { SkyConstants sky; float filter[4]; } constants{};
            constants.sky = parameters;
            compute.TransitionResource(mProceduralRadiance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            compute.TransitionResource(mProceduralIrradiance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
            for (uint32_t mip = 0; mip < 8; ++mip)
            {
                uint32_t size = 128u >> mip;
                constants.filter[0] = float(size); constants.filter[1] = float(mip) / 7;
                constants.filter[2] = mip == 0 ? 0.f : 2.f;
                compute.SetDynamicConstantBufferView(0, sizeof(constants), &constants);
                compute.SetDynamicDescriptor(1, 0, mProceduralRadiance.GetUAV(mip));
                compute.Dispatch3D(size, size, 6, 8, 8, 1);
            }
            constants.filter[0] = 16; constants.filter[1] = 1; constants.filter[2] = 1;
            compute.SetDynamicConstantBufferView(0, sizeof(constants), &constants);
            compute.SetDynamicDescriptor(1, 0, mProceduralIrradiance.GetUAV());
            compute.Dispatch3D(16, 16, 6, 8, 8, 1);
            compute.TransitionResource(mProceduralRadiance, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            compute.TransitionResource(mProceduralIrradiance, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
            mBakedConstants = parameters; mLightingReady = true; ++mLightingGeneration;
        }
        if (mBoundLighting != int(procedural) || mLightingSSAO != gSSAOFullScreen.GetVersionID() || mLightingShadow != gShadowBuffer.GetVersionID())
        {
            // 持久描述符只在光照源切换时替换，参数更新复用原资源和描述符。
            DX12Core::gCommandManager.IdleGPU();
            uint32_t count = 9, sourceCounts[] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
            D3D12_CPU_DESCRIPTOR_HANDLE sources[] = {
                procedural ? mProceduralRadiance.GetSRV() : mRadianceCubeMap.IsValid() ? mRadianceCubeMap.GetSRV() : GetDefaultTexture(kBlackCubeMap),
                procedural ? mProceduralIrradiance.GetSRV() : mIrradianceCubeMap.IsValid() ? mIrradianceCubeMap.GetSRV() : GetDefaultTexture(kBlackCubeMap),
                mBRDF_LUT.IsValid() ? mBRDF_LUT.GetSRV() : GetDefaultTexture(kDefaultBRDFLUT),
                gSSAOFullScreen.GetSRV(), gShadowBuffer.GetSRV(), LightManager::GetLightSrv(),
                LightManager::GetLightShadowSrv(), LightManager::GetLightGridSrv(), LightManager::GetLightGridBitMaskSrv()};
            DX12Core::gDevice->CopyDescriptors(1, &mLightingTextures, &count, count, sources, sourceCounts, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            mBoundLighting = int(procedural);
            mLightingSSAO = gSSAOFullScreen.GetVersionID(); mLightingShadow = gShadowBuffer.GetVersionID();
        }
    }

	void SkyboxRenderer::SetIBLTextures(const std::wstring& diffuseFile,const std::wstring& specularFile)
	{
		TextureRef specularIBL = specularFile.empty() ? TextureRef{} : AssetManager::LoadTextureFile(specularFile);
		TextureRef diffuseIBL = diffuseFile.empty() ? TextureRef{} : AssetManager::LoadTextureFile(diffuseFile);
        SetIBLTextures(diffuseIBL,specularIBL);
	}

	void SkyboxRenderer::SetIBLBias(float LODBias)
	{
		mSpecularIBLBias = std::clamp(LODBias, 0.f, ProceduralLighting() ? 7.f : mSpecularIBLRange);
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

        auto skyPSCB = Constants();
        auto mode = skyPSCB.ground[3];

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
		context.SetDynamicConstantBufferView(kMaterialConstants, sizeof(SkyConstants), &skyPSCB);
		context.SetDynamicDescriptor(2, 0, mode == 1 ? mEnvironmentMap.GetSRV() : GetDefaultTexture(kBlackCubeMap));
        context.SetDynamicDescriptor(2, 1, mode == 2 ? mEnvironmentMap.GetSRV() : GetDefaultTexture(kBlackOpaque2D));

		context.Draw(3);

	}

	float SkyboxRenderer::GetIBLRange()const
	{
		return (ProceduralLighting() ? 7.f : mSpecularIBLRange) - mSpecularIBLBias;
	}

	float SkyboxRenderer::GetIBLBias()const
	{
		return mSpecularIBLBias;
	}

}
