#pragma once
#include "Runtime/Platform/DirectX12/Buffer/GpuResource.h"

namespace AtomEngine
{
    class Texture : public GpuResource
    {
        friend class CommandContext;

    public:

        Texture() { mCpuDescriptorHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
        Texture(D3D12_CPU_DESCRIPTOR_HANDLE Handle) : mCpuDescriptorHandle(Handle) {}

        void Create2D(size_t RowPitchBytes, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitData);
        void CreateCube(size_t RowPitchBytes, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitialData);

        virtual void Destroy() override
        {
            GpuResource::Destroy();
            mCpuDescriptorHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return mCpuDescriptorHandle; }
        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }
        uint32_t GetDepth() const { return mDepth; }
        D3D12_SRV_DIMENSION GetSRVDimension() const { return mSrvDimension; }

    protected:

        uint32_t mWidth;
        uint32_t mHeight;
        uint32_t mDepth;
        D3D12_SRV_DIMENSION mSrvDimension = D3D12_SRV_DIMENSION_UNKNOWN;

        D3D12_CPU_DESCRIPTOR_HANDLE mCpuDescriptorHandle;
    };

}
