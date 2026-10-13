#include "ReadbackBuffer.h"
#include "../Core/DirectX12Core.h"

namespace AtomEngine
{
    using namespace DX12Core;

    void ReadbackBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize)
    {
        Destroy();

        mElementCount = NumElements;
        mElementSize = ElementSize;
        mBufferSize = NumElements * ElementSize;
        mUsageState = D3D12_RESOURCE_STATE_COPY_DEST;

        // すべてのテクセルデータを保持するのに十分な大きさのリードバックバッファを作成する
        D3D12_HEAP_PROPERTIES HeapProps;
        HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapProps.CreationNodeMask = 1;
        HeapProps.VisibleNodeMask = 1;

        // リードバックバッファは1次元である必要があります。「texture2d」ではなく「buffer」です。
        D3D12_RESOURCE_DESC ResourceDesc = {};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Width = mBufferSize;
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.SampleDesc.Quality = 0;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(gDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mResource)));

        mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

#ifdef _DEBUG
        mResource->SetName(name.c_str());

#else
        (name);
#endif
    }

    void* ReadbackBuffer::Map(void)
    {
        void* Memory;
        CD3DX12_RANGE range(0, mBufferSize);
        mResource->Map(0, &range, &Memory);
        return Memory;
    }

    void ReadbackBuffer::Unmap(void)
    {
        CD3DX12_RANGE range(0, 0);
        mResource->Unmap(0, &range);
    }

}
