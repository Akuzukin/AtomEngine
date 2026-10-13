#include "ColorBuffer.h"
#include "../Core/DirectX12Core.h"
#include "../Core/GraphicsCommon.h"
#include "../Context/ComputeContext.h"

namespace AtomEngine
{
    using namespace DX12Core;

    void ColorBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips)
    {
        ASSERT(ArraySize == 1 || NumMips == 1, "We don't support auto-mips on mTexture arrays");

        mNumMipMaps = NumMips - 1;

        D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

        RTVDesc.Format = Format;
        UAVDesc.Format = GetUAVFormat(Format);
        SRVDesc.Format = Format;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (ArraySize > 1)
        {
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            RTVDesc.Texture2DArray.MipSlice = 0;
            RTVDesc.Texture2DArray.FirstArraySlice = 0;
            RTVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

            UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            UAVDesc.Texture2DArray.MipSlice = 0;
            UAVDesc.Texture2DArray.FirstArraySlice = 0;
            UAVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            SRVDesc.Texture2DArray.MipLevels = NumMips;
            SRVDesc.Texture2DArray.MostDetailedMip = 0;
            SRVDesc.Texture2DArray.FirstArraySlice = 0;
            SRVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;
        }
        else if (mFragmentCount > 1)
        {
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        }
        else
        {
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            RTVDesc.Texture2D.MipSlice = 0;

            UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            UAVDesc.Texture2D.MipSlice = 0;

            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = NumMips;
            SRVDesc.Texture2D.MostDetailedMip = 0;
        }

        if (mSRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        {
            mRTVHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            mSRVHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        ID3D12Resource* Resource = mResource.Get();

        // rtvを作成
        Device->CreateRenderTargetView(Resource, &RTVDesc, mRTVHandle);

        // rtvを作成
        Device->CreateShaderResourceView(Resource, &SRVDesc, mSRVHandle);

        if (mFragmentCount > 1)
            return;

        // Create the UAVs for each mip level (RWTexture2D)
        for (uint32_t i = 0; i < NumMips; ++i)
        {
            if (mUAVHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
                mUAVHandle[i] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            Device->CreateUnorderedAccessView(Resource, nullptr, &UAVDesc, mUAVHandle[i]);

            UAVDesc.Texture2D.MipSlice++;
        }
    }

    void ColorBuffer::CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource)
    {
        AssociateWithResource(Name, BaseResource, D3D12_RESOURCE_STATE_PRESENT);

        mRTVHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        gDevice->CreateRenderTargetView(mResource.Get(), nullptr, mRTVHandle);
    }

    void ColorBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
        DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMem)
    {
        NumMips = (NumMips == 0 ? ComputeNumMips(Width, Height) : NumMips);
        D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
        D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, NumMips, Format, Flags);

        ResourceDesc.SampleDesc.Count = mFragmentCount;
        ResourceDesc.SampleDesc.Quality = 0;

        D3D12_CLEAR_VALUE ClearValue = {};
        ClearValue.Format = Format;
        ClearValue.Color[0] = mClearColor.r;
        ClearValue.Color[1] = mClearColor.g;
        ClearValue.Color[2] = mClearColor.b;
        ClearValue.Color[3] = mClearColor.a;

        CreateTextureResource(gDevice.Get(), Name, ResourceDesc, ClearValue, VidMem);
        CreateDerivedViews(gDevice.Get(), Format, 1, NumMips);
    }

    void ColorBuffer::CreateArray(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t ArrayCount,
        DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMem)
    {
        D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
        D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, ArrayCount, 1, Format, Flags);

        D3D12_CLEAR_VALUE ClearValue = {};
        ClearValue.Format = Format;
        ClearValue.Color[0] = mClearColor.r;
        ClearValue.Color[1] = mClearColor.g;
        ClearValue.Color[2] = mClearColor.b;
        ClearValue.Color[3] = mClearColor.a;

        CreateTextureResource(gDevice.Get(), Name, ResourceDesc, ClearValue, VidMem);
        CreateDerivedViews(gDevice.Get(), Format, ArrayCount, 1);
    }
}
