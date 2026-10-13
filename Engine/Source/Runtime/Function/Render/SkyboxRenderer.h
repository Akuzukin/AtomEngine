#pragma once
#pragma once
#include "Runtime/Resource/TextureRef.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"

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
		void Render(GraphicsContext& context, const Camera* camera, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor);
		float GetIBLRange()const;
		float GetIBLBias()const;

	private:
		TextureRef mEnvironmentMap;
		TextureRef mRadianceCubeMap;
		TextureRef mIrradianceCubeMap;
		TextureRef mBRDF_LUT;

		float mSpecularIBLRange = 0;
		float mSpecularIBLBias = 0;

		RootSignature mSkyboxRootSig;
		GraphicsPSO mSkyboxPSO;
	};
}
