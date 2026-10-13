#include "PipelineState.h"
#include "Runtime/Core/Utility/Hash.h"
#include "../Core/DirectX12Core.h"
#include <map>
#include <thread>
#include <mutex>

using Microsoft::WRL::ComPtr;

namespace AtomEngine
{
	using namespace DX12Core;

	static std::map< size_t, ComPtr<ID3D12PipelineState> > sGraphicsPSOHashMap;
	static std::map< size_t, ComPtr<ID3D12PipelineState> > sComputePSOHashMap;

	void PSO::DestroyAll(void)
	{
		sGraphicsPSOHashMap.clear();
		sComputePSOHashMap.clear();
	}


	GraphicsPSO::GraphicsPSO(const wchar_t* Name)
		: PSO(Name)
	{
		ZeroMemory(&mPSODesc, sizeof(mPSODesc));
		mPSODesc.NodeMask = 1;
		mPSODesc.SampleMask = 0xFFFFFFFFu;
		mPSODesc.SampleDesc.Count = 1;
		mPSODesc.InputLayout.NumElements = 0;
	}

	void GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
	{
		mPSODesc.BlendState = BlendDesc;
	}

	void GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
	{
		mPSODesc.RasterizerState = RasterizerDesc;
	}

	void GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
	{
		mPSODesc.DepthStencilState = DepthStencilDesc;
	}

	void GraphicsPSO::SetSampleMask(UINT SampleMask)
	{
		mPSODesc.SampleMask = SampleMask;
	}

	void GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
	{
		ASSERT(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefined topology");
		mPSODesc.PrimitiveTopologyType = TopologyType;
	}

	void GraphicsPSO::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps)
	{
		mPSODesc.IBStripCutValue = IBProps;
	}

	void GraphicsPSO::SetDepthTargetFormat(DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		SetRenderTargetFormats(0, nullptr, DSVFormat, MsaaCount, MsaaQuality);
	}

	void GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
	}

	void GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		ASSERT(NumRTVs == 0 || RTVFormats != nullptr, "Null format array conflicts with non-zero length");
		for (UINT i = 0; i < NumRTVs; ++i)
		{
			ASSERT(RTVFormats[i] != DXGI_FORMAT_UNKNOWN);
			mPSODesc.RTVFormats[i] = RTVFormats[i];
		}
		for (UINT i = NumRTVs; i < mPSODesc.NumRenderTargets; ++i)
			mPSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		mPSODesc.NumRenderTargets = NumRTVs;
		mPSODesc.DSVFormat = DSVFormat;
		mPSODesc.SampleDesc.Count = MsaaCount;
		mPSODesc.SampleDesc.Quality = MsaaQuality;
	}

	void GraphicsPSO::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
	{
		mPSODesc.InputLayout.NumElements = NumElements;

		if (NumElements > 0)
		{
			D3D12_INPUT_ELEMENT_DESC* NewElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
			memcpy(NewElements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
			mInputLayouts=std::shared_ptr<const D3D12_INPUT_ELEMENT_DESC>(NewElements,[](const D3D12_INPUT_ELEMENT_DESC*p){free(const_cast<D3D12_INPUT_ELEMENT_DESC*>(p));});
		}
		else
			mInputLayouts = nullptr;
	}

    void GraphicsPSO::Finalize(){
        mPSODesc.pRootSignature=mRootSignature->GetSignature();
        mPSODesc.InputLayout.pInputElementDescs=nullptr;
        size_t hash=HashState(&mPSODesc);hash=HashState(mInputLayouts.get(),mPSODesc.InputLayout.NumElements,hash);
        hash=HashState(static_cast<const uint32_t*>(mPSODesc.VS.pShaderBytecode),mPSODesc.VS.BytecodeLength/4,hash);
        hash=HashState(static_cast<const uint32_t*>(mPSODesc.PS.pShaderBytecode),mPSODesc.PS.BytecodeLength/4,hash);
        mPSODesc.InputLayout.pInputElementDescs=mInputLayouts.get();
        static std::mutex mutex;std::lock_guard<std::mutex>lock(mutex);
        auto found=sGraphicsPSOHashMap.find(hash);if(found!=sGraphicsPSOHashMap.end()){mPSO=found->second.Get();return;}
        ComPtr<ID3D12PipelineState>state;ThrowIfFailed(gDevice->CreateGraphicsPipelineState(&mPSODesc,IID_PPV_ARGS(&state)));state->SetName(mName);mPSO=state.Get();sGraphicsPSOHashMap.emplace(hash,std::move(state));
    }
    void ComputePSO::Finalize(){
        mPSODesc.pRootSignature=mRootSignature->GetSignature();size_t hash=HashState(&mPSODesc);hash=HashState(static_cast<const uint32_t*>(mPSODesc.CS.pShaderBytecode),mPSODesc.CS.BytecodeLength/4,hash);
        static std::mutex mutex;std::lock_guard<std::mutex>lock(mutex);auto found=sComputePSOHashMap.find(hash);if(found!=sComputePSOHashMap.end()){mPSO=found->second.Get();return;}
        ComPtr<ID3D12PipelineState>state;ThrowIfFailed(gDevice->CreateComputePipelineState(&mPSODesc,IID_PPV_ARGS(&state)));state->SetName(mName);mPSO=state.Get();sComputePSOHashMap.emplace(hash,std::move(state));
    }
    void GraphicsPSO::RefreshShaders(){
        auto v=mVertex?mVertex->Revision():0,p=mPixel?mPixel->Revision():0;if(v==mVertexRevision&&p==mPixelRevision)return;
        mVertexRevision=v;mPixelRevision=p;auto candidate=*this;
        if(mVertex){candidate.mVertexBlob=mVertex->Bytecode();candidate.mPSODesc.VS={candidate.mVertexBlob->GetBufferPointer(),candidate.mVertexBlob->GetBufferSize()};}
        if(mPixel){candidate.mPixelBlob=mPixel->Bytecode();candidate.mPSODesc.PS={candidate.mPixelBlob->GetBufferPointer(),candidate.mPixelBlob->GetBufferSize()};}
        try{candidate.Finalize();*this=std::move(candidate);}catch(const std::exception&e){ShaderLibrary::ReportError(std::string("Graphics pipeline reload failed: ")+e.what());}
    }
    void ComputePSO::RefreshShaders(){
        auto revision=mCompute?mCompute->Revision():0;if(revision==mRevision)return;mRevision=revision;auto candidate=*this;
        candidate.mComputeBlob=mCompute->Bytecode();candidate.mPSODesc.CS={candidate.mComputeBlob->GetBufferPointer(),candidate.mComputeBlob->GetBufferSize()};
        try{candidate.Finalize();*this=std::move(candidate);}catch(const std::exception&e){ShaderLibrary::ReportError(std::string("Compute pipeline reload failed: ")+e.what());}
    }
	ComputePSO::ComputePSO(const wchar_t* Name)
		: PSO(Name)
	{
		ZeroMemory(&mPSODesc, sizeof(mPSODesc));
		mPSODesc.NodeMask = 1;
	}
}
