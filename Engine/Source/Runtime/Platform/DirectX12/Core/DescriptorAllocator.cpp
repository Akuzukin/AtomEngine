#include "DescriptorAllocator.h"
#include "DirectX12Core.h"

namespace AtomEngine
{
    using namespace DX12Core;

    std::mutex DescriptorAllocator::mAllocationMutex;
    std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::mDescriptorHeapPool;

    void DescriptorAllocator::DestroyAll(void)
    {
        mDescriptorHeapPool.clear();
    }

    ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
    {
        std::lock_guard<std::mutex> LockGuard(mAllocationMutex);

        D3D12_DESCRIPTOR_HEAP_DESC Desc;
        Desc.Type = Type;
        Desc.NumDescriptors = kNumDescriptorsPerHeap;
        Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        Desc.NodeMask = 1;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
        ThrowIfFailed(gDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&pHeap)));
        mDescriptorHeapPool.emplace_back(pHeap);
        return pHeap.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t Count)
    {
        if (mCurrentHeap == nullptr || mRemainingFreeHandles < Count)
        {
            mCurrentHeap = RequestNewHeap(mType);
            mCurrentHandle = mCurrentHeap->GetCPUDescriptorHandleForHeapStart();
            mRemainingFreeHandles = kNumDescriptorsPerHeap;

            if (mDescriptorSize == 0)
                mDescriptorSize = gDevice->GetDescriptorHandleIncrementSize(mType);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE ret = mCurrentHandle;
        mCurrentHandle.ptr += Count * mDescriptorSize;
        mRemainingFreeHandles -= Count;
        return ret;
    }
}
