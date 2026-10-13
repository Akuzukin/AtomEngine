#pragma once
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
namespace AtomEngine
{
class FXAA
{
    static inline RootSignature root;
    static inline GraphicsPSO pipeline;

  public:
    static void Initialize()
    {
        root.Reset(2, 1);
        root.InitStaticSampler(0, SamplerLinearClampDesc, D3D12_SHADER_VISIBILITY_PIXEL);
        root[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
        root[1].InitAsConstants(0, 2, D3D12_SHADER_VISIBILITY_PIXEL);
        root.Finalize(L"FXAA");
        pipeline.SetRootSignature(root);
        pipeline.SetRasterizerState(RasterizerTwoSided);
        pipeline.SetBlendState(BlendDisable);
        pipeline.SetDepthStencilState(DepthStateDisabled);
        pipeline.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        pipeline.SetRenderTargetFormat(DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_UNKNOWN);
        pipeline.SetVertexShader(ShaderCompiler::CompileBlob(L"FullScreenQuad.hlsl", L"vs_6_2").Get());
        pipeline.SetPixelShader(ShaderCompiler::CompileBlob(L"PostEffects/FXAA.hlsl", L"ps_6_2").Get());
        pipeline.Finalize();
    }
    static void Render(GraphicsContext &context)
    {
        context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        context.TransitionResource(gFxaaBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        context.SetRenderTarget(gFxaaBuffer.GetRTV());
        context.SetViewportAndScissor(0, 0, gFxaaBuffer.GetWidth(), gFxaaBuffer.GetHeight());
        context.SetRootSignature(root);
        context.SetPipelineState(pipeline);
        context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context.SetDynamicDescriptor(0, 0, gSceneColorBuffer.GetSRV());
        context.SetConstants(1, 1.f / gFxaaBuffer.GetWidth(), 1.f / gFxaaBuffer.GetHeight());
        context.Draw(3);
    }
};
} // namespace AtomEngine
