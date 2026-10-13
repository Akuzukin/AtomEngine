#include "DepthBuffer.h"
#include "../Core/DirectX12Core.h"

namespace AtomEngine
{
    using namespace DX12Core;

    void DepthBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
    {
        Create(Name, Width, Height, 1, Format, VidMemPtr);
    }

    void DepthBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t Samples, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
    {
        D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, 1, Format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        ResourceDesc.SampleDesc.Count = Samples;

        D3D12_CLEAR_VALUE ClearValue = {};
        ClearValue.DepthStencil.Depth = mClearDepth;
        ClearValue.DepthStencil.Stencil = mClearStencil;
        ClearValue.Format = Format;
        CreateTextureResource(gDevice.Get(), Name, ResourceDesc, ClearValue, VidMemPtr);
        CreateDerivedViews(gDevice.Get(), Format);
    }

    void DepthBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format)
    {
        ID3D12Resource* Resource = mResource.Get();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Format = GetDSVFormat(Format);
        if (Resource->GetDesc().SampleDesc.Count == 1)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        }

        if (mHandleDSV[0].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        {
            mHandleDSV[0] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            mHandleDSV[1] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        }

        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        Device->CreateDepthStencilView(Resource, &dsvDesc, mHandleDSV[0]);

        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
        Device->CreateDepthStencilView(Resource, &dsvDesc, mHandleDSV[1]);

        DXGI_FORMAT stencilReadFormat = GetStencilFormat(Format);
        if (stencilReadFormat != DXGI_FORMAT_UNKNOWN)
        {
            if (mHandleDSV[2].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            {
                mHandleDSV[2] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
                mHandleDSV[3] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            }

            dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL;
            Device->CreateDepthStencilView(Resource, &dsvDesc, mHandleDSV[2]);

            dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
            Device->CreateDepthStencilView(Resource, &dsvDesc, mHandleDSV[3]);
        }
        else
        {
            mHandleDSV[2] = mHandleDSV[0];
            mHandleDSV[3] = mHandleDSV[1];
        }

        if (mHandleDepthSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            mHandleDepthSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // srvを作成
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = GetDepthFormat(Format);
        if (dsvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
        {
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = 1;
        }
        else
        {
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        }
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        Device->CreateShaderResourceView(Resource, &SRVDesc, mHandleDepthSRV);

        if (stencilReadFormat != DXGI_FORMAT_UNKNOWN)
        {
            if (mHandleStencilSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
                mHandleStencilSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            SRVDesc.Format = stencilReadFormat;
            SRVDesc.Texture2D.PlaneSlice = 1;

            Device->CreateShaderResourceView(Resource, &SRVDesc, mHandleStencilSRV);
        }
    }

}
