#pragma once
#pragma once
#include "Runtime/Resource/TextureRef.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Platform/DirectX12/Buffer/ColorBuffer.h"

namespace AtomEngine
{
	class SkyboxRenderer
	{
	public:
		void Initialize();
		void Shutdown();

		void SetEnvironmentMap(TextureRef environmentMap);
		void SetIBLTextures(TextureRef diffuseIBL, TextureRef specularIBL);
		void SetBRDF_LUT(const std::wstring& BRDF_LUT_File);

		void SetEnvironmentMap(const std::wstring& file);
		void SetIBLTextures(const std::wstring& diffuseFile, const std::wstring& specularFile);

		void SetIBLBias(float LODBias);
        void SetSkySettings(const SceneSettings &settings, Vector3 sunDirection = Vector3(.3f, .65f, .7f),
                            Vector3 sunColor = Vector3(1.f, .95f, .8f));
        const std::string &TextureError() const { return mTextureError; }
        // 将程序化天空卷积到 HDR cubemap，并绑定到 PBR 像素着色器的 t10/t11。
        void UpdateEnvironmentLighting(GraphicsContext &context);
        const DescriptorHandle &LightingTextures() const { return mLightingTextures; }
        uint64_t LightingGeneration() const { return mLightingGeneration; }
		void Render(GraphicsContext& context, const Camera* camera, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor);
		float GetIBLRange()const;
		float GetIBLBias()const;

	private:
		TextureRef mEnvironmentMap;
		TextureRef mRadianceCubeMap;
		TextureRef mIrradianceCubeMap;
		TextureRef mBRDF_LUT;
        SceneSettings mSettings;
        Vector3 mSkySunDirection{.3f, .65f, .7f}, mSkySunColor{1.f, .95f, .8f};
        std::string mTextureError;
        struct alignas(16) SkyConstants { float zenith[4], horizon[4], ground[4], sun[4], sunColor[4]; };
        SkyConstants Constants() const;
        SkyConstants mBakedConstants{};
        ColorBuffer mProceduralRadiance, mProceduralIrradiance;
        RootSignature mLightingRootSig;
        ComputePSO mLightingPSO;
        bool mLightingReady = false;
        int mBoundLighting = -1;
        uint64_t mLightingGeneration = 0;
        DescriptorHandle mLightingTextures;
        uint32_t mLightingSSAO = 0, mLightingShadow = 0;
        bool ProceduralLighting() const { return mSettings.skyMode == 0 || !mEnvironmentMap.IsValid(); }

		float mSpecularIBLRange = 0;
		float mSpecularIBLBias = 0;

		RootSignature mSkyboxRootSig;
		GraphicsPSO mSkyboxPSO;
	};
}
