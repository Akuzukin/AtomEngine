#include "ComputeContext.h"
#include "../Buffer/ColorBuffer.h"
#include "../Core/DirectX12Core.h"

namespace AtomEngine
{
    ComputeContext& ComputeContext::Begin(const std::wstring& ID, bool Async)
    {
        ComputeContext& NewContext = DX12Core::gContextManager.AllocateContext(
            Async ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT)->GetComputeContext();
        NewContext.SetID(ID);

        return NewContext;
    }

    void ComputeContext::ClearUAV(GpuBuffer& Target)
    {
        FlushResourceBarriers();

        //UAV をバインドした後、UAV としてクリアするために必要な
        // GPU ハンドルを取得できます(基本的に、すべての値を設定するシェーダーを実行するため)。
        D3D12_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = mDynamicViewDescriptorHeap.UploadDirect(Target.GetUAV());
        const UINT ClearColor[4] = {};
        mCommandList->ClearUnorderedAccessViewUint(GpuVisibleHandle, Target.GetUAV(), Target.GetResource(), ClearColor, 0, nullptr);
    }

    void ComputeContext::ClearUAV(ColorBuffer& Target)
    {
        FlushResourceBarriers();

        // UAV をバインドした後、UAV としてクリアするために必要な
        // GPU ハンドルを取得できます（基本的にシェーダーを実行してすべての値を設定するため）。
        D3D12_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = mDynamicViewDescriptorHeap.UploadDirect(Target.GetUAV());
        CD3DX12_RECT ClearRect(0, 0, (LONG)Target.GetWidth(), (LONG)Target.GetHeight());

        const float* ClearColor = Target.GetClearColor().ptr();
        mCommandList->ClearUnorderedAccessViewFloat(GpuVisibleHandle, Target.GetUAV(), Target.GetResource(), ClearColor, 1, &ClearRect);
    }

}
