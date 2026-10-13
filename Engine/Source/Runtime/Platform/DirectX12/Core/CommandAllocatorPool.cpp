#include "CommandAllocatorPool.h"

namespace AtomEngine
{
    CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE Type) :
        mCommandListType(Type),
        mDevice(nullptr)
    {
    }

    CommandAllocatorPool::~CommandAllocatorPool()
    {
        Shutdown();
    }

    void CommandAllocatorPool::Create(ID3D12Device* pDevice)
    {
        mDevice = pDevice;
    }

    void CommandAllocatorPool::Shutdown()
    {
        for (size_t i = 0; i < mAllocatorPool.size(); ++i)
            mAllocatorPool[i]->Release();

        mAllocatorPool.clear();
    }

    ID3D12CommandAllocator* CommandAllocatorPool::RequestAllocator(uint64_t CompletedFenceValue)
    {
        std::lock_guard<std::mutex> LockGuard(mAllocatorMutex);

        ID3D12CommandAllocator* pAllocator = nullptr;

        if (!mReadyAllocators.empty())
        {
            std::pair<uint64_t, ID3D12CommandAllocator*>& AllocatorPair = mReadyAllocators.front();

            if (AllocatorPair.first <= CompletedFenceValue)
            {
                pAllocator = AllocatorPair.second;
                ThrowIfFailed(pAllocator->Reset());
                mReadyAllocators.pop();
            }
        }

        // 利用できるアロケータがない場合は、新しいアロケータを作成します。
        if (pAllocator == nullptr)
        {
            ThrowIfFailed(mDevice->CreateCommandAllocator(mCommandListType, IID_PPV_ARGS(&pAllocator)));
            wchar_t AllocatorName[32];
            swprintf(AllocatorName, 32, L"CommandAllocator %zu", mAllocatorPool.size());
            pAllocator->SetName(AllocatorName);
            mAllocatorPool.push_back(pAllocator);
        }

        return pAllocator;
    }

    void CommandAllocatorPool::DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* Allocator)
    {
        std::lock_guard<std::mutex> LockGuard(mAllocatorMutex);

        //このフェンス値は、アロケータを自由にリセットできることを示しています
        mReadyAllocators.push(std::make_pair(FenceValue, Allocator));
    }

}
