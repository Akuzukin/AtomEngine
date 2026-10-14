#include "Texture.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Context/CommandContext.h"
#include "DirectXTex.h"
#include <map>
#include <thread>

using namespace DirectX;
namespace AtomEngine
{
    using namespace DX12Core;
    void Texture::Create2D(size_t RowPitchBytes, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitialData)
    {
        Destroy();

        mUsageState = D3D12_RESOURCE_STATE_COPY_DEST;

        mWidth = (uint32_t)Width;
        mHeight = (uint32_t)Height;
        mDepth = 1;
        mSrvDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = Width;
        texDesc.Height = (UINT)Height;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = Format;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES HeapProps;
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapProps.CreationNodeMask = 1;
        HeapProps.VisibleNodeMask = 1;

        ThrowIfFailed(gDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            mUsageState, nullptr, IID_PPV_ARGS(mResource.ReleaseAndGetAddressOf())));

        mResource->SetName(L"Texture");

        D3D12_SUBRESOURCE_DATA texResource;
        texResource.pData = InitialData;
        texResource.RowPitch = RowPitchBytes;
        texResource.SlicePitch = RowPitchBytes * Height;

        CommandContext::InitializeTexture(*this, 1, &texResource);

        if (mCpuDescriptorHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            mCpuDescriptorHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        gDevice->CreateShaderResourceView(mResource.Get(), nullptr, mCpuDescriptorHandle);
    }

    void Texture::CreateCube(size_t RowPitchBytes, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitialData)
    {
        Destroy();

        mUsageState = D3D12_RESOURCE_STATE_COPY_DEST;

        mWidth = (uint32_t)Width;
        mHeight = (uint32_t)Height;
        mDepth = 6;
        mSrvDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = Width;
        texDesc.Height = (UINT)Height;
        texDesc.DepthOrArraySize = 6;
        texDesc.MipLevels = 1;
        texDesc.Format = Format;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES HeapProps;
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapProps.CreationNodeMask = 1;
        HeapProps.VisibleNodeMask = 1;

        ThrowIfFailed(gDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            mUsageState, nullptr, IID_PPV_ARGS(mResource.ReleaseAndGetAddressOf())));

        mResource->SetName(L"Texture");

        D3D12_SUBRESOURCE_DATA texResource;
        texResource.pData = InitialData;
        texResource.RowPitch = RowPitchBytes;
        texResource.SlicePitch = texResource.RowPitch * Height;

        CommandContext::InitializeTexture(*this, 1, &texResource);

        if (mCpuDescriptorHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            mCpuDescriptorHandle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = Format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, mCpuDescriptorHandle);
    }
}
