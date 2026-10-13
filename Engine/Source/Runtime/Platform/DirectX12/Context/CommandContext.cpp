#include "CommandContext.h"
#include "../Buffer/ColorBuffer.h"
#include "../Buffer/DepthBuffer.h"
#include "../Buffer/UploadBuffer.h"
#include "../Buffer/ReadbackBuffer.h"
#include "../Core/DirectX12Core.h"
#include "../Core/DescriptorHeap.h"

namespace AtomEngine
{
    using namespace DX12Core;
    void ContextManager::DestroyAllContexts(void)
    {
        for (uint32_t i = 0; i < 4; ++i)
            mContextPool[i].clear();
    }

    CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
    {
        std::lock_guard<std::mutex> LockGuard(mContextAllocationMutex);

        auto& AvailableContexts = mAvailableContexts[Type];

        CommandContext* ret = nullptr;
        if (AvailableContexts.empty())
        {
            ret = new CommandContext(Type);
            mContextPool[Type].emplace_back(ret);
            ret->Initialize();
        }
        else
        {
            ret = AvailableContexts.front();
            AvailableContexts.pop();
            ret->Reset();
        }
        ASSERT(ret != nullptr);

        ASSERT(ret->mType == Type);

        return ret;
    }

    void ContextManager::FreeContext(CommandContext* UsedContext)
    {
        ASSERT(UsedContext != nullptr);
        std::lock_guard<std::mutex> LockGuard(mContextAllocationMutex);
        mAvailableContexts[UsedContext->mType].push(UsedContext);
    }

    void CommandContext::DestroyAllContexts(void)
    {
        LinearAllocator::DestroyAll();
        DynamicDescriptorHeap::DestroyAll();
        DX12Core::gContextManager.DestroyAllContexts();
    }

    CommandContext& CommandContext::Begin(const std::wstring ID)
    {
        CommandContext* NewContext = DX12Core::gContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
        NewContext->SetID(ID);
        return *NewContext;
    }

    uint64_t CommandContext::Flush(bool WaitForCompletion)
    {
        FlushResourceBarriers();

        ASSERT(mCurrentAllocator != nullptr);

        uint64_t FenceValue = gCommandManager.GetQueue(mType).ExecuteCommandList(mCommandList);

        if (WaitForCompletion)
            gCommandManager.WaitForFence(FenceValue);

   
        mCommandList->Reset(mCurrentAllocator, nullptr);

        if (mCurGraphicsRootSignature)
        {
            mCommandList->SetGraphicsRootSignature(mCurGraphicsRootSignature);
        }
        if (mCurComputeRootSignature)
        {
            mCommandList->SetComputeRootSignature(mCurComputeRootSignature);
        }
        if (mCurPipelineState)
        {
            mCommandList->SetPipelineState(mCurPipelineState);
        }

        BindDescriptorHeaps();

        return FenceValue;
    }

    uint64_t CommandContext::Finish(bool WaitForCompletion)
    {
        ASSERT(mType == D3D12_COMMAND_LIST_TYPE_DIRECT || mType == D3D12_COMMAND_LIST_TYPE_COMPUTE);

        FlushResourceBarriers();

        ASSERT(mCurrentAllocator != nullptr);

        CommandQueue& Queue = gCommandManager.GetQueue(mType);

        uint64_t FenceValue = Queue.ExecuteCommandList(mCommandList);
        Queue.DiscardAllocator(FenceValue, mCurrentAllocator);
        mCurrentAllocator = nullptr;

        mCpuLinearAllocator.CleanupUsedPages(FenceValue);
        mGpuLinearAllocator.CleanupUsedPages(FenceValue);
        mDynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue);
        mDynamicSamplerDescriptorHeap.CleanupUsedHeaps(FenceValue);

        if (WaitForCompletion)
            gCommandManager.WaitForFence(FenceValue);

        gContextManager.FreeContext(this);

        return FenceValue;
    }

    CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE Type) :
        mType(Type),
        mDynamicViewDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
        mDynamicSamplerDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
        mCpuLinearAllocator(kCpuWritable),
        mGpuLinearAllocator(kGpuExclusive)
    {
        mOwningManager = nullptr;
        mCommandList = nullptr;
        mCurrentAllocator = nullptr;
        ZeroMemory(mCurrentDescriptorHeaps, sizeof(mCurrentDescriptorHeaps));

        mCurGraphicsRootSignature = nullptr;
        mCurComputeRootSignature = nullptr;
        mCurPipelineState = nullptr;
        mNumBarriersToFlush = 0;
    }

    CommandContext::~CommandContext(void)
    {
        if (mCommandList != nullptr)
            mCommandList->Release();
    }

    void CommandContext::Initialize(void)
    {
        gCommandManager.CreateNewCommandList(mType, &mCommandList, &mCurrentAllocator);
    }

    void CommandContext::Reset(void)
    {
        //Reset() は、以前に解放されたコンテキストに対してのみ呼び出します。コマンドリストは保持されますが、
        // 新しいアロケータを要求する必要があります。
        ASSERT(mCommandList != nullptr && mCurrentAllocator == nullptr);
        mCurrentAllocator = gCommandManager.GetQueue(mType).RequestAllocator();
        mCommandList->Reset(mCurrentAllocator, nullptr);

        mCurGraphicsRootSignature = nullptr;
        mCurComputeRootSignature = nullptr;
        mCurPipelineState = nullptr;
        mNumBarriersToFlush = 0;

        BindDescriptorHeaps();
    }

    void CommandContext::BindDescriptorHeaps(void)
    {
        UINT NonNullHeaps = 0;
        ID3D12DescriptorHeap* HeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
        for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            ID3D12DescriptorHeap* HeapIter = mCurrentDescriptorHeaps[i];
            if (HeapIter != nullptr)
                HeapsToBind[NonNullHeaps++] = HeapIter;
        }

        if (NonNullHeaps > 0)
            mCommandList->SetDescriptorHeaps(NonNullHeaps, HeapsToBind);
    }

    void CommandContext::TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
    {
        D3D12_RESOURCE_STATES OldState = Resource.mUsageState;

        if (mType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            ASSERT((OldState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == OldState);
            ASSERT((NewState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == NewState);
        }

        if (OldState != NewState)
        {
            // Keep chained transitions (including split barriers) in separate batches.
            // Their states are ordered; submitting both for the same subresource in
            // one ResourceBarrier call triggers the debug layer's duplicate warning.
            for (uint32_t i = 0; i < mNumBarriersToFlush; ++i)
            {
                const auto& pending = mResourceBarrierBuffer[i];
                if (pending.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                    pending.Transition.pResource == Resource.GetResource())
                {
                    FlushResourceBarriers();
                    break;
                }
            }
            ASSERT(mNumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
            D3D12_RESOURCE_BARRIER& BarrierDesc = mResourceBarrierBuffer[mNumBarriersToFlush++];

            BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            BarrierDesc.Transition.pResource = Resource.GetResource();
            BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            BarrierDesc.Transition.StateBefore = OldState;
            BarrierDesc.Transition.StateAfter = NewState;

            if (NewState == Resource.mTransitioningState)
            {
                BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
                Resource.mTransitioningState = (D3D12_RESOURCE_STATES)-1;
            }
            else
                BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            Resource.mUsageState = NewState;
        }
        else if (NewState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            InsertUAVBarrier(Resource, FlushImmediate);

        if (FlushImmediate || mNumBarriersToFlush == 16)
            FlushResourceBarriers();
    }

    void CommandContext::BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
    {
        // すでに遷移中の場合は、その遷移を終了します
        if (Resource.mTransitioningState != (D3D12_RESOURCE_STATES)-1)
            TransitionResource(Resource, Resource.mTransitioningState);

        D3D12_RESOURCE_STATES OldState = Resource.mUsageState;

        if (OldState != NewState)
        {
            for (uint32_t i = 0; i < mNumBarriersToFlush; ++i)
            {
                const auto& pending = mResourceBarrierBuffer[i];
                if (pending.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                    pending.Transition.pResource == Resource.GetResource())
                {
                    FlushResourceBarriers();
                    break;
                }
            }
            ASSERT(mNumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
            D3D12_RESOURCE_BARRIER& BarrierDesc = mResourceBarrierBuffer[mNumBarriersToFlush++];

            BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            BarrierDesc.Transition.pResource = Resource.GetResource();
            BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            BarrierDesc.Transition.StateBefore = OldState;
            BarrierDesc.Transition.StateAfter = NewState;

            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;

            Resource.mTransitioningState = NewState;
        }

        if (FlushImmediate || mNumBarriersToFlush == 16)
            FlushResourceBarriers();
    }

    void CommandContext::InsertUAVBarrier(GpuResource& Resource, bool FlushImmediate)
    {
        ASSERT(mNumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
        D3D12_RESOURCE_BARRIER& BarrierDesc = mResourceBarrierBuffer[mNumBarriersToFlush++];

        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        BarrierDesc.UAV.pResource = Resource.GetResource();

        if (FlushImmediate)
            FlushResourceBarriers();
    }

    void CommandContext::InsertAliasBarrier(GpuResource& Before, GpuResource& After, bool FlushImmediate)
    {
        ASSERT(mNumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
        D3D12_RESOURCE_BARRIER& BarrierDesc = mResourceBarrierBuffer[mNumBarriersToFlush++];

        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        BarrierDesc.Aliasing.pResourceBefore = Before.GetResource();
        BarrierDesc.Aliasing.pResourceAfter = After.GetResource();

        if (FlushImmediate)
            FlushResourceBarriers();
    }

    void CommandContext::WriteBuffer(GpuResource& Dest, size_t DestOffset, const void* BufferData, size_t NumBytes)
    {
        ASSERT(BufferData != nullptr && IsAligned(BufferData, 16));
        DynAlloc TempSpace = mCpuLinearAllocator.Allocate(NumBytes, 512);
        SIMDMemCopy(TempSpace.DataPtr, BufferData, DivideByMultiple(NumBytes, 16));
        CopyBufferRegion(Dest, DestOffset, TempSpace.Buffer, TempSpace.Offset, NumBytes);
    }

    void CommandContext::FillBuffer(GpuResource& Dest, size_t DestOffset, DWParam Value, size_t NumBytes)
    {
        DynAlloc TempSpace = mCpuLinearAllocator.Allocate(NumBytes, 512);
        __m128 VectorValue = _mm_set1_ps(Value.Float);
        SIMDMemFill(TempSpace.DataPtr, VectorValue, DivideByMultiple(NumBytes, 16));
        CopyBufferRegion(Dest, DestOffset, TempSpace.Buffer, TempSpace.Offset, NumBytes);
    }

    void CommandContext::InitializeTexture(GpuResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[])
    {
        UINT64 uploadBufferSize = GetRequiredIntermediateSize(Dest.GetResource(), 0, NumSubresources);

        CommandContext& InitContext = CommandContext::Begin();

        // データを中間アップロードヒープにコピーし、アップロードヒープからデフォルトテクスチャへのコピーをスケジュールします。
        DynAlloc mem = InitContext.ReserveUploadMemory(uploadBufferSize);
        UpdateSubresources(InitContext.mCommandList, Dest.GetResource(), mem.Buffer.GetResource(), 0, 0, NumSubresources, SubData);
        InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ);

        // コマンドリストを実行し、アップロードバッファを解放できるように完了するまで待ちます。
        InitContext.Finish(true);
    }

    void CommandContext::CopySubresource(GpuResource& Dest, UINT DestSubIndex, GpuResource& Src, UINT SrcSubIndex)
    {
        FlushResourceBarriers();

        D3D12_TEXTURE_COPY_LOCATION DestLocation =
        {
            Dest.GetResource(),
            D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            DestSubIndex
        };

        D3D12_TEXTURE_COPY_LOCATION SrcLocation =
        {
            Src.GetResource(),
            D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            SrcSubIndex
        };

        mCommandList->CopyTextureRegion(&DestLocation, 0, 0, 0, &SrcLocation, nullptr);
    }

    void CommandContext::InitializeTextureArraySlice(GpuResource& Dest, UINT SliceIndex, GpuResource& Src)
    {
        CommandContext& Context = CommandContext::Begin();

        Context.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
        Context.FlushResourceBarriers();

        const D3D12_RESOURCE_DESC& DestDesc = Dest.GetResource()->GetDesc();
        const D3D12_RESOURCE_DESC& SrcDesc = Src.GetResource()->GetDesc();

        ASSERT(SliceIndex < DestDesc.DepthOrArraySize &&
            SrcDesc.DepthOrArraySize == 1 &&
            DestDesc.Width == SrcDesc.Width &&
            DestDesc.Height == SrcDesc.Height &&
            DestDesc.MipLevels <= SrcDesc.MipLevels
        );

        UINT SubResourceIndex = SliceIndex * DestDesc.MipLevels;

        for (UINT i = 0; i < DestDesc.MipLevels; ++i)
        {
            D3D12_TEXTURE_COPY_LOCATION destCopyLocation =
            {
                Dest.GetResource(),
                D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                SubResourceIndex + i
            };

            D3D12_TEXTURE_COPY_LOCATION srcCopyLocation =
            {
                Src.GetResource(),
                D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                i
            };

            Context.mCommandList->CopyTextureRegion(&destCopyLocation, 0, 0, 0, &srcCopyLocation, nullptr);
        }

        Context.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ);
        Context.Finish(true);
    }

    uint32_t CommandContext::ReadbackTexture(ReadbackBuffer& DstBuffer, PixelBuffer& SrcBuffer)
    {
        uint64_t CopySize = 0;

        // フットプリントはリソースのデバイスによって異なる場合がありますが、デバイスは 1 つだけであると想定します。
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedFootprint;
        const auto& desc = SrcBuffer.GetResource()->GetDesc();
        DX12Core::gDevice->GetCopyableFootprints(&desc, 0, 1, 0,
            &PlacedFootprint, nullptr, nullptr, &CopySize);

        DstBuffer.Create(L"Readback", (uint32_t)CopySize, 1);

        TransitionResource(SrcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, true);

        CD3DX12_TEXTURE_COPY_LOCATION Dst(DstBuffer.GetResource(), PlacedFootprint);
        CD3DX12_TEXTURE_COPY_LOCATION Src(SrcBuffer.GetResource(), 0);

        mCommandList->CopyTextureRegion(
            &Dst, 0, 0, 0,
            &Src, nullptr);

        return PlacedFootprint.Footprint.RowPitch;
    }

    void CommandContext::InitializeBuffer(GpuBuffer& Dest, const void* BufferData, size_t NumBytes, size_t DestOffset)
    {
        CommandContext& InitContext = CommandContext::Begin();

        DynAlloc mem = InitContext.ReserveUploadMemory(NumBytes);
        SIMDMemCopy(mem.DataPtr, BufferData,DivideByMultiple(NumBytes, 16));

        // データを中間アップロードヒープにコピーし、アップロードヒープからデフォルトテクスチャへのコピーをスケジュールします
        InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
        InitContext.mCommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, mem.Buffer.GetResource(), 0, NumBytes);
        InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

        // コマンドリストを実行し、完了するまで待機してアップロードバッファを解放します
        InitContext.Finish(true);
    }

    void CommandContext::InitializeBuffer(GpuBuffer& Dest, const UploadBuffer& Src, size_t SrcOffset, size_t NumBytes, size_t DestOffset)
    {
        CommandContext& InitContext = CommandContext::Begin();

        size_t MaxBytes = std::min<size_t>(Dest.GetBufferSize() - DestOffset, Src.GetBufferSize() - SrcOffset);
        NumBytes = std::min<size_t>(MaxBytes, NumBytes);

        // データを中間アップロードヒープにコピーし、アップロードヒープからデフォルトテクスチャへのコピーをスケジュールします
        InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
        InitContext.mCommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, (ID3D12Resource*)Src.GetResource(), SrcOffset, NumBytes);
        InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

        // コマンドリストを実行し、アップロードバッファを解放できるように完了するまで待ちます。
        InitContext.Finish(true);
    }
}
