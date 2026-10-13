#include "GraphicsContext.h"
#include "../Buffer/ColorBuffer.h"
#include "../Buffer/DepthBuffer.h"
#include "../Buffer/UploadBuffer.h"
#include "../Buffer/ReadbackBuffer.h"

namespace AtomEngine
{
    void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
    {
        mCommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, &DSV);
    }

    void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[])
    {
        mCommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, nullptr);
    }

    void GraphicsContext::BeginQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex)
    {
        mCommandList->BeginQuery(QueryHeap, Type, HeapIndex);
    }

    void GraphicsContext::EndQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex)
    {
        mCommandList->EndQuery(QueryHeap, Type, HeapIndex);
    }

    void GraphicsContext::ResolveQueryData(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource* DestinationBuffer, UINT64 DestinationBufferOffset)
    {
        mCommandList->ResolveQueryData(QueryHeap, Type, StartIndex, NumQueries, DestinationBuffer, DestinationBufferOffset);
    }

    void GraphicsContext::ClearUAV(GpuBuffer& Target)
    {
        FlushResourceBarriers();

        // UAV をバインドした後、UAV としてクリアするために必要な GPU ハンドルを取得できます 
        // (基本的に、すべての値を設定するシェーダーを実行するため)。
        D3D12_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = mDynamicViewDescriptorHeap.UploadDirect(Target.GetUAV());
        const UINT ClearColor[4] = {};
        mCommandList->ClearUnorderedAccessViewUint(GpuVisibleHandle, Target.GetUAV(), Target.GetResource(), ClearColor, 0, nullptr);
    }

    void GraphicsContext::ClearUAV(ColorBuffer& Target)
    {
        FlushResourceBarriers();

        // UAV をバインドした後、UAV としてクリアするために必要な GPU ハンドルを取得できます 
        // (基本的に、すべての値を設定するシェーダーを実行するため)。
        D3D12_GPU_DESCRIPTOR_HANDLE GpuVisibleHandle = mDynamicViewDescriptorHeap.UploadDirect(Target.GetUAV());
        CD3DX12_RECT ClearRect(0, 0, (LONG)Target.GetWidth(), (LONG)Target.GetHeight());

        const float* ClearColor = Target.GetClearColor().ptr();
        mCommandList->ClearUnorderedAccessViewFloat(GpuVisibleHandle, Target.GetUAV(), Target.GetResource(), ClearColor, 1, &ClearRect);
    }

    void GraphicsContext::ClearColor(ColorBuffer& Target, D3D12_RECT* Rect)
    {
        FlushResourceBarriers();
        mCommandList->ClearRenderTargetView(Target.GetRTV(), Target.GetClearColor().ptr(), (Rect == nullptr) ? 0 : 1, Rect);
    }

    void GraphicsContext::ClearColor(ColorBuffer& Target, float Colour[4], D3D12_RECT* Rect)
    {
        FlushResourceBarriers();
        mCommandList->ClearRenderTargetView(Target.GetRTV(), Colour, (Rect == nullptr) ? 0 : 1, Rect);
    }

    void GraphicsContext::ClearDepth(DepthBuffer& Target)
    {
        FlushResourceBarriers();
        mCommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
    }

    void GraphicsContext::ClearStencil(DepthBuffer& Target)
    {
        FlushResourceBarriers();
        mCommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_STENCIL, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
    }

    void GraphicsContext::ClearDepthAndStencil(DepthBuffer& Target)
    {
        FlushResourceBarriers();
        mCommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
    }

    void GraphicsContext::SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect)
    {
        ASSERT(rect.left < rect.right && rect.top < rect.bottom);
        mCommandList->RSSetViewports(1, &vp);
        mCommandList->RSSetScissorRects(1, &rect);
    }

    void GraphicsContext::SetViewport(const D3D12_VIEWPORT& vp)
    {
        mCommandList->RSSetViewports(1, &vp);
    }

    void GraphicsContext::SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth, FLOAT maxDepth)
    {
        D3D12_VIEWPORT vp;
        vp.Width = w;
        vp.Height = h;
        vp.MinDepth = minDepth;
        vp.MaxDepth = maxDepth;
        vp.TopLeftX = x;
        vp.TopLeftY = y;
        mCommandList->RSSetViewports(1, &vp);
    }
    void GraphicsContext::SetScissor(const D3D12_RECT& rect)
    {
        ASSERT(rect.left < rect.right && rect.top < rect.bottom);
        mCommandList->RSSetScissorRects(1, &rect);
    }
}
