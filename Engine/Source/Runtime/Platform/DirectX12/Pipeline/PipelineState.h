#pragma once
#include"RootSignature.h"
#include<dxcapi.h>
#include "Runtime/Resource/Shader.h"

namespace AtomEngine
{
	class PSO
	{
	public:

		PSO(const wchar_t* Name) : mName(Name), mRootSignature(nullptr), mPSO(nullptr) {}

		static void DestroyAll(void);

		void SetRootSignature(const RootSignature& BindMappings)
		{
			mRootSignature = &BindMappings;
		}

		const RootSignature& GetRootSignature(void) const
		{
			ASSERT(mRootSignature != nullptr);
			return *mRootSignature;
		}

		virtual ~PSO()=default;
        virtual void RefreshShaders() {}
        ID3D12PipelineState* GetPipelineStateObject(void) const { const_cast<PSO*>(this)->RefreshShaders();return mPSO; }

	protected:

		const wchar_t* mName;

		const RootSignature* mRootSignature;

		ID3D12PipelineState* mPSO;
	};

	class GraphicsPSO : public PSO
	{
		friend class CommandContext;

	public:

		GraphicsPSO(const wchar_t* Name = L"Unnamed Graphics PSO");

		void SetBlendState(const D3D12_BLEND_DESC& BlendDesc);
		void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc);
		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc);
		void SetSampleMask(UINT SampleMask);
		void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType);
		void SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
		void SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
		void SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);
		void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps);
		void SetDepthTargetFormat(DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);

		// これらのconst_castsは必須ではないはずですが、APIを修正して「const void* pShaderBytecode」を受け入れるようにする必要があります。
		void SetVertexShader(const void* Binary, size_t Size) { mPSODesc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetPixelShader(const void* Binary, size_t Size) { mPSODesc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetGeometryShader(const void* Binary, size_t Size) { mPSODesc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetHullShader(const void* Binary, size_t Size) { mPSODesc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetDomainShader(const void* Binary, size_t Size) { mPSODesc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }

		void SetVertexShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.VS = Binary; }
		void SetPixelShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.PS = Binary; }
		void SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.GS = Binary; }
		void SetHullShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.HS = Binary; }
		void SetDomainShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.DS = Binary; }

		void SetVertexShader(IDxcBlob* blob) { mVertex=ShaderLibrary::FindBytecode(blob);mVertexBlob=blob;mVertexRevision=mVertex?mVertex->Revision():0;mPSODesc.VS = { blob->GetBufferPointer(),blob->GetBufferSize() }; }
		void SetPixelShader(IDxcBlob* blob) { mPixel=ShaderLibrary::FindBytecode(blob);mPixelBlob=blob;mPixelRevision=mPixel?mPixel->Revision():0;mPSODesc.PS = { blob->GetBufferPointer(),blob->GetBufferSize() }; }
		// パイプラインを生成
		void Finalize();

	private:

		void RefreshShaders()override;
        std::shared_ptr<Shader>mVertex,mPixel;
        Microsoft::WRL::ComPtr<IDxcBlob>mVertexBlob,mPixelBlob;
        uint64_t mVertexRevision=0,mPixelRevision=0;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC mPSODesc;
		std::shared_ptr<const D3D12_INPUT_ELEMENT_DESC> mInputLayouts;
	};

	class ComputePSO : public PSO
	{
		friend class CommandContext;
	public:
		ComputePSO(const wchar_t* Name = L"Unnamed Compute PSO");

		void SetComputeShader(const void* Binary, size_t Size) { mPSODesc.CS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetComputeShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.CS = Binary; }
		void SetComputeShader(IDxcBlob* blob) { mCompute=ShaderLibrary::FindBytecode(blob);mComputeBlob=blob;mRevision=mCompute?mCompute->Revision():0;mPSODesc.CS = { blob->GetBufferPointer(),blob->GetBufferSize() }; }

		void Finalize();

	private:

		void RefreshShaders()override;
        std::shared_ptr<Shader>mCompute;
        Microsoft::WRL::ComPtr<IDxcBlob>mComputeBlob;
        uint64_t mRevision=0;
		D3D12_COMPUTE_PIPELINE_STATE_DESC mPSODesc;
	};


}


