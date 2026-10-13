#include "SamplerManager.h"
#include "Runtime/Core/Utility/hash.h"
#include "DirectX12Core.h"
#include <map>

namespace
{
    std::map< size_t, D3D12_CPU_DESCRIPTOR_HANDLE > sSamplerCache;
}

namespace AtomEngine
{
    using namespace DX12Core;

    D3D12_CPU_DESCRIPTOR_HANDLE SamplerDesc::CreateDescriptor()
    {
        size_t hashValue = HashState(this);
        auto iter = sSamplerCache.find(hashValue);
        if (iter != sSamplerCache.end())
        {
            return iter->second;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE Handle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        gDevice->CreateSampler(this, Handle);
        return Handle;
    }

    void SamplerDesc::CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE Handle)
    {
        ASSERT(Handle.ptr != 0 && Handle.ptr != -1);
        gDevice->CreateSampler(this, Handle);
    }

}
