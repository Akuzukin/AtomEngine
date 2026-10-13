#pragma once
#include <map>
#include <vector>
#include <mutex>
#include "../D3dUtility/d3dInclude.h"

namespace AtomEngine
{
    class DescriptorHandle
    {
    public:
        DescriptorHandle()
        {
            mCpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
            mGpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        }

        DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle)
            : mCpuHandle(CpuHandle), mGpuHandle(GpuHandle)
        {
        }

        DescriptorHandle operator+ (INT OffsetScaledByDescriptorSize) const
        {
            DescriptorHandle ret = *this;
            ret += OffsetScaledByDescriptorSize;
            return ret;
        }

        void operator += (INT OffsetScaledByDescriptorSize)
        {
            if (mCpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
                mCpuHandle.ptr += OffsetScaledByDescriptorSize;
            if (mGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
                mGpuHandle.ptr += OffsetScaledByDescriptorSize;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE* operator&() const { return &mCpuHandle; }
        operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return mCpuHandle; }
        operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return mGpuHandle; }

        size_t GetCpuPtr() const { return mCpuHandle.ptr; }
        uint64_t GetGpuPtr() const { return mGpuHandle.ptr; }
        bool IsNull() const { return mCpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
        bool IsShaderVisible() const { return mGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }

    private:
        D3D12_CPU_DESCRIPTOR_HANDLE mCpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
    };

    class DescriptorHeap
    {
    public:

        DescriptorHeap(void) {}
        ~DescriptorHeap(void) { Destroy(); }

        void Create(const std::wstring& DebugHeapName, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount);
        void Destroy(void) { mHeap = nullptr; mFreeRanges.clear(); mPending.clear(); mAllocations.clear(); mNumFreeDescriptors = 0; }

        bool HasAvailableSpace(uint32_t Count) const { return Count <= mNumFreeDescriptors; }
        DescriptorHandle Alloc(uint32_t Count = 1);
        void Free(DescriptorHandle handle);
        uint32_t GetCapacity() const { return mHeapDesc.NumDescriptors; }
        uint32_t GetFreeCount() const { return mNumFreeDescriptors; }

        DescriptorHandle operator[] (uint32_t arrayIdx) const { return mFirstHandle + arrayIdx * mDescriptorSize; }

        uint32_t GetOffsetOfHandle(const DescriptorHandle& DHandle)
        {
            return (uint32_t)(DHandle.GetCpuPtr() - mFirstHandle.GetCpuPtr()) / mDescriptorSize;
        }

        bool ValidateHandle(const DescriptorHandle& DHandle) const;

        ID3D12DescriptorHeap* GetHeapPointer() const { return mHeap.Get(); }

        uint32_t GetDescriptorSize(void) const { return mDescriptorSize; }

    private:

        struct PendingRange { uint32_t offset, count; uint64_t graphics, compute, copy; };
        std::mutex mMutex;
        std::map<uint32_t, uint32_t> mFreeRanges;
        std::map<uint32_t, uint32_t> mAllocations;
        std::vector<PendingRange> mPending;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap = nullptr;
        D3D12_DESCRIPTOR_HEAP_DESC mHeapDesc;
        uint32_t mDescriptorSize = 0;
        uint32_t mNumFreeDescriptors = 0;
        DescriptorHandle mFirstHandle;
        DescriptorHandle mNextFreeHandle;
    };
}
