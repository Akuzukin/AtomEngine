#include "UiRenderer.h"
#include "UiSystem.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/SpriteRenderer.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include <dwrite.h>
#include <wrl.h>
#include <map>
#include <algorithm>
#include <cstring>
#pragma comment(lib, "dwrite.lib")

namespace AtomEngine
{
namespace
{
using Microsoft::WRL::ComPtr;
Texture white;
RootSignature root;
GraphicsPSO pso;
ComPtr<IDWriteFactory> factory;
struct CachedText
{
    Texture texture;
};
std::map<std::string, std::unique_ptr<CachedText>> textCache;
std::map<std::string, TextureRef> images;
class TextRenderer final : public IDWriteTextRenderer
{
    ULONG refs = 1;
    std::vector<uint32_t> &pixels;
    uint32_t width, height;

  public:
    TextRenderer(std::vector<uint32_t> &p, uint32_t w, uint32_t h) : pixels(p), width(w), height(h)
    {
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override
    {
        if (id == __uuidof(IUnknown) || id == __uuidof(IDWriteTextRenderer) || id == __uuidof(IDWritePixelSnapping))
        {
            *out = this;
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++refs;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        auto r = --refs;
        if (!r)
            delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *, BOOL *v) override
    {
        *v = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *, DWRITE_MATRIX *m) override
    {
        *m = {1, 0, 0, 1, 0, 0};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *, FLOAT *v) override
    {
        *v = 1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void *, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mode,
                                           const DWRITE_GLYPH_RUN *run, const DWRITE_GLYPH_RUN_DESCRIPTION *,
                                           IUnknown *) override
    {
        ComPtr<IDWriteGlyphRunAnalysis> analysis;
        HRESULT result =
            factory->CreateGlyphRunAnalysis(run, 1, nullptr, DWRITE_RENDERING_MODE_NATURAL, mode, x, y, &analysis);
        if (FAILED(result))
            return result;
        RECT bounds{};
        result = analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds);
        if (FAILED(result))
            return result;
        int w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
        if (w <= 0 || h <= 0)
            return S_OK;
        std::vector<uint8_t> alpha(size_t(w) * h * 3);
        result =
            analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds, alpha.data(), uint32_t(alpha.size()));
        if (FAILED(result))
            return result;
        for (int row = 0; row < h; ++row)
            for (int col = 0; col < w; ++col)
            {
                int px = col + bounds.left, py = row + bounds.top;
                if (px < 0 || py < 0 || px >= int(width) || py >= int(height))
                    continue;
                size_t offset = (size_t(row) * w + col) * 3;
                uint32_t a = std::max({alpha[offset], alpha[offset + 1], alpha[offset + 2]});
                auto &pixel = pixels[size_t(py) * width + px];
                pixel = 0x00ffffff | (std::max(a, pixel >> 24) << 24);
            }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *, IUnknown *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *, IUnknown *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL, BOOL,
                                               IUnknown *) override
    {
        return E_NOTIMPL;
    }
};
D3D12_CPU_DESCRIPTOR_HANDLE Text(const UiElementComponent &c)
{
    uint32_t width = uint32_t(std::clamp(c.size.x, 1.f, 4096.f)), height = uint32_t(std::clamp(c.size.y, 1.f, 4096.f));
    auto key = c.text + "|" + std::to_string(c.fontSize) + "|" + std::to_string(width) + "|" + std::to_string(height);
    if (auto it = textCache.find(key); it != textCache.end())
        return it->second->texture.GetSRV();
    std::vector<uint32_t> pixels(size_t(width) * height, 0x00ffffff);
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, c.text.c_str(), int(c.text.size()), nullptr, 0);
    if (count == 0)
        throw std::runtime_error("UI text must be valid UTF-8");
    std::wstring text(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, c.text.c_str(), int(c.text.size()), text.data(), count);
    ComPtr<IDWriteTextFormat> format;
    ThrowIfFailed(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                            DWRITE_FONT_STRETCH_NORMAL, std::clamp(c.fontSize, 1.f, 256.f), L"zh-CN",
                                            &format));
    ComPtr<IDWriteTextLayout> layout;
    ThrowIfFailed(factory->CreateTextLayout(text.c_str(), count, format.Get(), float(width), float(height), &layout));
    ComPtr<TextRenderer> renderer;
    renderer.Attach(new TextRenderer(pixels, width, height));
    ThrowIfFailed(layout->Draw(nullptr, renderer.Get(), 0, 0));

    auto item = std::make_unique<CachedText>();
    item->texture.Create2D(width * 4, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, pixels.data());
    auto srv = item->texture.GetSRV();
    textCache.emplace(key, std::move(item));
    return srv;
}
} // namespace
void UiRenderer::Initialize()
{
    uint32_t pixel = 0xffffffff;
    white.Create2D(4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &pixel);
    ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                      reinterpret_cast<IUnknown **>(factory.GetAddressOf())));
    root.Reset(2, 1);
    SamplerDesc sampler;
    root.InitStaticSampler(0, sampler, D3D12_SHADER_VISIBILITY_PIXEL);
    root[0].InitAsConstantBuffer(0);
    root[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    root.Finalize(L"UI Root", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    pso.SetRootSignature(root);
    pso.SetRasterizerState(RasterizerTwoSided);
    pso.SetBlendState(BlendTraditional);
    pso.SetDepthStencilState(DepthStateDisabled);
    pso.SetInputLayout(2, layout);
    pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    pso.SetRenderTargetFormat(gOverlayBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);
    pso.SetVertexShader(ShaderCompiler::CompileBlob(L"SpriteVS.hlsl", L"vs_6_2").Get());
    pso.SetPixelShader(ShaderCompiler::CompileBlob(L"SpritePS.hlsl", L"ps_6_2").Get());
    pso.Finalize();
}
void UiRenderer::Render(GraphicsContext &ctx, World &w)
{
    if (textCache.size() > 256)
    {
        DX12Core::gCommandManager.IdleGPU();
        textCache.clear();
    }
    ctx.SetRootSignature(root);
    ctx.SetPipelineState(pso);
    ctx.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (auto e : UiSystem::DrawOrder(w))
    {
        auto c = w.GetComponent<UiElementComponent>(e);
        auto rect=UiSystem::Resolve(w,e);c.position=rect.position;c.size=rect.size;c.fontSize*=rect.fontScale;
        if (!c.visible)
            continue;
        auto draw = [&](D3D12_CPU_DESCRIPTOR_HANDLE srv, const Color &color) {
            float x = c.position.x, y = c.position.y, sx = c.size.x, sy = c.size.y;
            alignas(16) SpriteVertex vertices[6] = {{{x, y, 0}, {0, 0}},           {{x + sx, y, 0}, {1, 0}},
                                                    {{x, y + sy, 0}, {0, 1}},      {{x + sx, y, 0}, {1, 0}},
                                                    {{x + sx, y + sy, 0}, {1, 1}}, {{x, y + sy, 0}, {0, 1}}};
            alignas(16) SpriteConstants constants{};
            constants.viewProj = Matrix4x4::IDENTITY;
            constants.viewProj[0][0] = 2.f / gOverlayBuffer.GetWidth();
            constants.viewProj[1][1] = -2.f / gOverlayBuffer.GetHeight();
            constants.viewProj[3][0] = -1;
            constants.viewProj[3][1] = 1;
            constants.uvTransform = Matrix4x4::IDENTITY;
            constants.color = color.ToVector4();
            ctx.SetDynamicConstantBufferView(0, sizeof(constants), &constants);
            ctx.SetDynamicDescriptor(1, 0, srv);
            ctx.SetDynamicVB(0, 6, sizeof(SpriteVertex), vertices);
            ctx.Draw(6);
        };
        if(c.interactable || c.progressBar || c.background.a>0)draw(white.GetSRV(),e==w.uiFocus && c.interactable?Color(.3f,.5f,.8f,1):c.background);
        if(c.progressBar){auto full=c.size.x;c.size.x*=std::clamp(c.progress,0.f,1.f);draw(white.GetSRV(),c.color);c.size.x=full;}
        if (!c.texture.empty())
        {
            auto it = images.find(c.texture);
            if (it == images.end())
                it = images.emplace(c.texture, AssetManager::LoadCovertTexture(c.texture, kWhiteOpaque2D, true)).first;
            draw(it->second.GetSRV(), c.color);
        }
        if (!c.text.empty())
            draw(Text(c), c.color);
    }
}
void UiRenderer::Shutdown()
{
    white.Destroy();
    textCache.clear();
    images.clear();
    factory.Reset();
}
} // namespace AtomEngine
