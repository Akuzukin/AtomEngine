#include "ShadowBuffer.h"
#include "../Context/GraphicsContext.h"

namespace AtomEngine
{
    void ShadowBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
    {
        DepthBuffer::Create(Name, Width, Height, DXGI_FORMAT_D16_UNORM, VidMemPtr);

        mViewport.TopLeftX = 0.0f;
        mViewport.TopLeftY = 0.0f;
        mViewport.Width = (float)Width;
        mViewport.Height = (float)Height;
        mViewport.MinDepth = 0.0f;
        mViewport.MaxDepth = 1.0f;

        // 影が伸びるのを心配しなくて済むように境界ピクセルへの描画を防ぐ
        mScissor.left = 1;
        mScissor.top = 1;
        mScissor.right = (LONG)Width - 2;
        mScissor.bottom = (LONG)Height - 2;
    }

    void ShadowBuffer::BeginRendering(GraphicsContext& Context)
    {
        Context.TransitionResource(*this, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
        Context.ClearDepth(*this);
        Context.SetDepthStencilTarget(GetDSV());
        Context.SetViewportAndScissor(mViewport, mScissor);
    }

    void ShadowBuffer::EndRendering(GraphicsContext& Context)
    {
        Context.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

}
