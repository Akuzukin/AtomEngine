#include "LinearAllocator.h"
#include "../Core/CommandListManager.h"
#include "../Core/DirectX12Core.h"

namespace AtomEngine
{
    using namespace DX12Core;

    LinearAllocatorType LinearAllocatorPageManager::sAutoType = kGpuExclusive;

    LinearAllocatorPageManager::LinearAllocatorPageManager()
    {
        m_AllocationType = sAutoType;
        sAutoType = (LinearAllocatorType)(sAutoType + 1);
        ASSERT(sAutoType <= kNumAllocatorTypes);
    }

    LinearAllocatorPageManager LinearAllocator::sPageManager[2];

    LinearAllocationPage* LinearAllocatorPageManager::RequestPage()
    {
        std::lock_guard<std::mutex> LockGuard(mMutex);

        while (!mRetiredPages.empty() && gCommandManager.IsFenceComplete(mRetiredPages.front().first))
        {
            mAvailablePages.push(mRetiredPages.front().second);
            mRetiredPages.pop();
        }

        LinearAllocationPage* PagePtr = nullptr;

        if (!mAvailablePages.empty())
        {
            PagePtr = mAvailablePages.front();
            mAvailablePages.pop();
        }
        else
        {
            PagePtr = CreateNewPage();
            mPagePool.emplace_back(PagePtr);
        }

        return PagePtr;
    }

    void LinearAllocatorPageManager::DiscardPages(uint64_t FenceValue, const std::vector<LinearAllocationPage*>& UsedPages)
    {
        std::lock_guard<std::mutex> LockGuard(mMutex);
        for (auto iter = UsedPages.begin(); iter != UsedPages.end(); ++iter)
            mRetiredPages.push(std::make_pair(FenceValue, *iter));
    }

    void LinearAllocatorPageManager::FreeLargePages(uint64_t FenceValue, const std::vector<LinearAllocationPage*>& LargePages)
    {
        std::lock_guard<std::mutex> LockGuard(mMutex);

        while (!mDeletionQueue.empty() && gCommandManager.IsFenceComplete(mDeletionQueue.front().first))
        {
            delete mDeletionQueue.front().second;
            mDeletionQueue.pop();
        }

        for (auto iter = LargePages.begin(); iter != LargePages.end(); ++iter)
        {
            (*iter)->Unmap();
            mDeletionQueue.push(std::make_pair(FenceValue, *iter));
        }
    }

    LinearAllocationPage* LinearAllocatorPageManager::CreateNewPage(size_t PageSize)
    {
        D3D12_HEAP_PROPERTIES HeapProps;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapProps.CreationNodeMask = 1;
        HeapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDesc;
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Alignment = 0;
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.SampleDesc.Quality = 0;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_RESOURCE_STATES DefaultUsage;

        if (m_AllocationType == kGpuExclusive)
        {
            HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            ResourceDesc.Width = PageSize == 0 ? kGpuAllocatorPageSize : PageSize;
            ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            DefaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        else
        {
            HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            ResourceDesc.Width = PageSize == 0 ? kCpuAllocatorPageSize : PageSize;
            ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        ID3D12Resource* pBuffer;
        ThrowIfFailed(gDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
            &ResourceDesc, DefaultUsage, nullptr, IID_PPV_ARGS(&pBuffer)));

        pBuffer->SetName(L"LinearAllocator Page");

        return new LinearAllocationPage(pBuffer, DefaultUsage);
    }

    void LinearAllocator::CleanupUsedPages(uint64_t FenceID)
    {
        if (mCurPage != nullptr)
        {
            mRetiredPage.push_back(mCurPage);
            mCurPage = nullptr;
            mCurOffset = 0;
        }

        sPageManager[mAllocationType].DiscardPages(FenceID, mRetiredPage);
        mRetiredPage.clear();

        sPageManager[mAllocationType].FreeLargePages(FenceID, mLargePageList);
        mLargePageList.clear();
    }

    DynAlloc LinearAllocator::AllocateLargePage(size_t SizeInBytes)
    {
        LinearAllocationPage* OneOff = sPageManager[mAllocationType].CreateNewPage(SizeInBytes);
        mLargePageList.push_back(OneOff);

        DynAlloc ret(*OneOff, 0, SizeInBytes);
        ret.DataPtr = OneOff->mCpuVirtualAddress;
        ret.GpuAddress = OneOff->mGpuVirtualAddress;

        return ret;
    }

    DynAlloc LinearAllocator::Allocate(size_t SizeInBytes, size_t Alignment)
    {
        const size_t AlignmentMask = Alignment - 1;

        // アライメントを確認
        ASSERT((AlignmentMask & Alignment) == 0);

        // 配分を揃える
        const size_t AlignedSize = AlignUpWithMask(SizeInBytes, AlignmentMask);

        if (AlignedSize > mPageSize)
            return AllocateLargePage(AlignedSize);

        mCurOffset = AlignUp(mCurOffset, Alignment);

        if (mCurOffset + AlignedSize > mPageSize)
        {
            ASSERT(mCurPage != nullptr);
            mRetiredPage.push_back(mCurPage);
            mCurPage = nullptr;
        }

        if (mCurPage == nullptr)
        {
            mCurPage = sPageManager[mAllocationType].RequestPage();
            mCurOffset = 0;
        }

        DynAlloc ret(*mCurPage, mCurOffset, AlignedSize);
        ret.DataPtr = (uint8_t*)mCurPage->mCpuVirtualAddress + mCurOffset;
        ret.GpuAddress = mCurPage->mGpuVirtualAddress + mCurOffset;

        mCurOffset += AlignedSize;

        return ret;
    }

}
