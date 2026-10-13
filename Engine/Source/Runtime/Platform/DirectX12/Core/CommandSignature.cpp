#include "CommandSignature.h"
#include "../Pipeline/RootSignature.h"
#include "../Core/DirectX12Core.h"

namespace AtomEngine
{
    using namespace DX12Core;

    void CommandSignature::Finalize(const RootSignature* RootSignature)
    {
        if (mFinalized)
            return;

        UINT ByteStride = 0;
        bool RequiresRootSignature = false;

        for (UINT i = 0; i < kNumParameters; ++i)
        {
            switch (mParamArray[i].GetDesc().Type)
            {
            case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
                ByteStride += sizeof(D3D12_DRAW_ARGUMENTS);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
                ByteStride += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
                ByteStride += sizeof(D3D12_DISPATCH_ARGUMENTS);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
                ByteStride += mParamArray[i].GetDesc().Constant.Num32BitValuesToSet * 4;
                RequiresRootSignature = true;
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
                ByteStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
                ByteStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
                ByteStride += 8;
                RequiresRootSignature = true;
                break;
            }
        }

        D3D12_COMMAND_SIGNATURE_DESC CommandSignatureDesc;
        CommandSignatureDesc.ByteStride = ByteStride;
        CommandSignatureDesc.NumArgumentDescs = kNumParameters;
        CommandSignatureDesc.pArgumentDescs = (const D3D12_INDIRECT_ARGUMENT_DESC*)mParamArray.get();
        CommandSignatureDesc.NodeMask = 1;

        Microsoft::WRL::ComPtr<ID3DBlob> pOutBlob, pErrorBlob;

        ID3D12RootSignature* pRootSig = RootSignature ? RootSignature->GetSignature() : nullptr;
        if (RequiresRootSignature)
        {
            ASSERT(pRootSig != nullptr);
        }
        else
        {
            pRootSig = nullptr;
        }

        ThrowIfFailed(gDevice->CreateCommandSignature(&CommandSignatureDesc, pRootSig,
            IID_PPV_ARGS(&mSignature)));

        mSignature->SetName(L"CommandSignature");

        mFinalized = TRUE;
    }
}
