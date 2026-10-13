#include "DescriptorHeap.h"
#include "DirectX12Core.h"
#include "CommandListManager.h"
#include <stdexcept>

namespace AtomEngine
{
    using namespace DX12Core;

    void DescriptorHeap::Create(const std::wstring& Name, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount)
    {
        mHeapDesc.Type = Type;
        mHeapDesc.NumDescriptors = MaxCount;
        mHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        mHeapDesc.NodeMask = 1;

        ThrowIfFailed(gDevice->CreateDescriptorHeap(&mHeapDesc, IID_PPV_ARGS(mHeap.ReleaseAndGetAddressOf())));

#ifndef _DEBUG
        (void)Name;
#else
        mHeap->SetName(Name.c_str());
#endif

        mDescriptorSize = gDevice->GetDescriptorHandleIncrementSize(mHeapDesc.Type);
        mNumFreeDescriptors = mHeapDesc.NumDescriptors;
        mFirstHandle = DescriptorHandle(
            mHeap->GetCPUDescriptorHandleForHeapStart(),
            mHeap->GetGPUDescriptorHandleForHeapStart());
        mNextFreeHandle = mFirstHandle;
        mFreeRanges = {{0, MaxCount}};
        mPending.clear();
        mAllocations.clear();
    }

    DescriptorHandle DescriptorHeap::Alloc(uint32_t Count)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto& manager = DX12Core::gCommandManager;
        for (auto it = mPending.begin(); it != mPending.end();) {
            if (!manager.IsFenceComplete(it->graphics) || !manager.IsFenceComplete(it->compute) || !manager.IsFenceComplete(it->copy)) { ++it; continue; }
            mFreeRanges.emplace(it->offset, it->count);
            mNumFreeDescriptors += it->count;
            it = mPending.erase(it);
        }
        // Coalesce retired adjacent blocks before searching for a contiguous table.
        for (auto it = mFreeRanges.begin(); it != mFreeRanges.end();) {
            auto next = std::next(it);
            if (next != mFreeRanges.end() && it->first + it->second == next->first) {
                it->second += next->second; mFreeRanges.erase(next);
            } else it = next;
        }
        if (!Count) throw std::invalid_argument("Descriptor allocation must be nonzero");
        for (auto it = mFreeRanges.begin(); it != mFreeRanges.end(); ++it) {
            if (it->second < Count) continue;
            const auto offset = it->first, available = it->second;
            mFreeRanges.erase(it);
            if (available > Count) mFreeRanges.emplace(offset + Count, available - Count);
            mAllocations.emplace(offset, Count);
            mNumFreeDescriptors -= Count;
            return mFirstHandle + offset * mDescriptorSize;
        }
        throw std::runtime_error("Shader-visible descriptor heap exhausted");
    }

    void DescriptorHeap::Free(DescriptorHandle handle)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mHeap || !ValidateHandle(handle)) return;
        const auto offset = GetOffsetOfHandle(handle);
        auto it = mAllocations.find(offset);
        if (it == mAllocations.end()) return;
        auto& manager = DX12Core::gCommandManager;
        mPending.push_back({offset, it->second,
            manager.GetGraphicsQueue().IncrementFence(),
            manager.GetComputeQueue().IncrementFence(),
            manager.GetCopyQueue().IncrementFence()});
        mAllocations.erase(it);
    }

    bool DescriptorHeap::ValidateHandle(const DescriptorHandle& DHandle) const
    {
        if (DHandle.GetCpuPtr() < mFirstHandle.GetCpuPtr() ||
            DHandle.GetCpuPtr() >= mFirstHandle.GetCpuPtr() + mHeapDesc.NumDescriptors * mDescriptorSize)
            return false;

        if (DHandle.GetGpuPtr() - mFirstHandle.GetGpuPtr() !=
            DHandle.GetCpuPtr() - mFirstHandle.GetCpuPtr())
            return false;

        return true;
    }
}
