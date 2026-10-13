#pragma once
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
inline float SmokeSRGB(float x)
{
    return x <= .0031308f ? 12.92f * x : 1.055f * powf(x, 1.f / 2.4f) - .055f;
}
inline void RunColorSpaceSmokeChecks()
{
    using namespace AtomEngine;
    ColorBuffer scene, overlay, result;
    scene.SetClearColor(Color(.25f, .25f, .25f, 1));
    scene.Create(L"Color regression scene", 32, 32, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
    result.Create(L"Color regression result", 32, 32, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
    RootSignature root;
    root.Reset(2, 1);
    root[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
    root[1].InitAsConstants(0, 2);
    root.InitStaticSampler(0, SamplerLinearClampDesc);
    root.Finalize(L"Color regression");
    auto vs = ShaderCompiler::CompileBlob(L"FullScreenQuad.hlsl", L"vs_6_2");
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto folder = std::filesystem::path(exe).parent_path() / "Smoke";
    for (bool scaled : {false, true})
    {
        auto ps = ShaderCompiler::CompileBlob(scaled ? L"ScaleAndCompositePS.hlsl" : L"CompositePS.hlsl", L"ps_6_2");
        GraphicsPSO pipeline;
        pipeline.SetRootSignature(root);
        pipeline.SetRasterizerState(RasterizerTwoSided);
        pipeline.SetBlendState(BlendDisable);
        pipeline.SetDepthStencilState(DepthStateDisabled);
        pipeline.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        pipeline.SetRenderTargetFormat(result.GetFormat(), DXGI_FORMAT_UNKNOWN);
        pipeline.SetVertexShader(vs.Get());
        pipeline.SetPixelShader(ps.Get());
        pipeline.Finalize();
        for (int mode = 0; mode < 3; ++mode)
        {
            float alpha = mode == 0 ? 0 : mode == 1 ? .5f : 1, straight = .5f;
            overlay.SetClearColor(Color(straight * alpha, straight * alpha, straight * alpha, alpha));
            overlay.Create(L"Color regression overlay", 32, 32, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
            auto &context = GraphicsContext::Begin(L"Linear UI composition regression");
            context.TransitionResource(scene, D3D12_RESOURCE_STATE_RENDER_TARGET);
            context.TransitionResource(overlay, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
            context.ClearColor(scene);
            context.ClearColor(overlay);
            context.TransitionResource(scene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            context.TransitionResource(overlay, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            context.TransitionResource(result, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
            context.SetRenderTarget(result.GetRTV());
            context.SetViewportAndScissor(0, 0, 32, 32);
            context.SetRootSignature(root);
            context.SetPipelineState(pipeline);
            context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context.SetConstants(1, .7071f / 32, .7071f / 32);
            D3D12_CPU_DESCRIPTOR_HANDLE views[] = {scene.GetSRV(), overlay.GetSRV()};
            context.SetDynamicDescriptors(0, 0, 2, views);
            context.Draw(3);
            context.Finish(true);
            auto path = folder / (std::string("linear-overlay-") + (scaled ? "scaled-" : "native-") +
                                  std::to_string(mode) + ".raw");
            result.ExportToFile(path.wstring());
            std::ifstream file(path, std::ios::binary);
            uint32_t header[4]{};
            file.read(reinterpret_cast<char *>(header), 16);
            std::vector<uint32_t> pixels(size_t(header[1] / 4) * header[3]);
            file.read(reinterpret_cast<char *>(pixels.data()), pixels.size() * 4);
            float sceneLinear = roundf(.25f * 1023) / 1023.f, overlayLinear = roundf(straight * alpha * 255) / 255.f,
                  storedAlpha = roundf(alpha * 255) / 255.f;
            float expected = SmokeSRGB(overlayLinear + sceneLinear * (1 - storedAlpha));
            FeatureCheck(std::abs(float(pixels[(header[1] / 4) * 16 + 16] & 1023) / 1023.f - expected) < .003f,
                         "Overlay gamma/alpha composition is incorrect");
        }
    }
    WriteLog(LogLevel::Info, "Smoke",
             "Linear overlay blending and single sRGB conversion passed (native/scaled; transparent/half/opaque)");
}
